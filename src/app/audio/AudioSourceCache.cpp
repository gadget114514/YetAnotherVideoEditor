#include "AudioSourceCache.h"

#include "../../core/AssetLibrary.h"
#include "../../core/Project.h"

#include <algorithm>

#if defined(YAVE_HAVE_FFMPEG)
extern "C" {
#  include <libavcodec/avcodec.h>
#  include <libavformat/avformat.h>
#  include <libavutil/channel_layout.h>
#  include <libavutil/samplefmt.h>
#  include <libswresample/swresample.h>
}
#endif

namespace yave::app {

namespace {

#if defined(YAVE_HAVE_FFMPEG)

/// ファイル全体を float プラナー PCM へデコードする。
/// ターゲットレート (プロジェクト sampleRate) へリサンプリングし、最大 2 ch にする。
std::shared_ptr<audio::DecodedAudio> decodeFileToPcm(const QString& path, int targetRate)
{
    AVFormatContext* fmt = nullptr;
    if (avformat_open_input(&fmt, path.toUtf8().constData(), nullptr, nullptr) < 0 || !fmt)
        return nullptr;
    struct FmtGuard
    {
        AVFormatContext* c;
        ~FmtGuard() { if (c) avformat_close_input(&c); }
    } fmtGuard{fmt};

    if (avformat_find_stream_info(fmt, nullptr) < 0)
        return nullptr;

    const int streamIndex = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIndex < 0)
        return nullptr;
    AVStream* st = fmt->streams[streamIndex];

    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!codec)
        return nullptr;

    AVCodecContext* cc = avcodec_alloc_context3(codec);
    if (!cc)
        return nullptr;
    struct CcGuard
    {
        AVCodecContext* c;
        ~CcGuard() { if (c) avcodec_free_context(&c); }
    } ccGuard{cc};
    if (avcodec_parameters_to_context(cc, st->codecpar) < 0)
        return nullptr;
    if (avcodec_open2(cc, codec, nullptr) < 0)
        return nullptr;

    const int srcRate = cc->sample_rate > 0 ? cc->sample_rate : 48000;
    const int outRate = targetRate > 0 ? targetRate : srcRate;
    const int outCh   = std::min(2, cc->ch_layout.nb_channels > 0 ? cc->ch_layout.nb_channels : 2);

    SwrContext* swr = nullptr;
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, outCh);
    const int swrRet = swr_alloc_set_opts2(&swr, &outLayout, AV_SAMPLE_FMT_FLTP, outRate,
                                           &cc->ch_layout, cc->sample_fmt, srcRate, 0, nullptr);
    av_channel_layout_uninit(&outLayout);
    if (swrRet < 0 || !swr)
        return nullptr;
    struct SwrGuard
    {
        SwrContext* c;
        ~SwrGuard() { if (c) swr_free(&c); }
    } swrGuard{swr};
    if (swr_init(swr) < 0)
        return nullptr;

    auto decoded = std::make_shared<audio::DecodedAudio>();
    decoded->sampleRate = outRate;
    decoded->channels   = outCh;
    std::vector<std::vector<float>> ch{size_t(outCh)};

    AVPacket* pkt = av_packet_alloc();
    AVFrame*  fr  = av_frame_alloc();
    struct PktGuard
    {
        AVPacket* c;
        ~PktGuard() { if (c) av_packet_free(&c); }
    } pktGuard{pkt};
    struct FrGuard
    {
        AVFrame* c;
        ~FrGuard() { if (c) av_frame_free(&c); }
    } frGuard{fr};
    if (!pkt || !fr)
        return nullptr;

    while (av_read_frame(fmt, pkt) >= 0) {
        if (pkt->stream_index != streamIndex) {
            av_packet_unref(pkt);
            continue;
        }
        if (avcodec_send_packet(cc, pkt) < 0) {
            av_packet_unref(pkt);
            continue;
        }
        av_packet_unref(pkt);

        while (true) {
            const int ret = avcodec_receive_frame(cc, fr);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
                break;

            const int outSamples = swr_get_out_samples(swr, fr->nb_samples);
            if (outSamples <= 0) {
                av_frame_unref(fr);
                continue;
            }
            std::vector<std::vector<uint8_t>> scratch{size_t(outCh)};
            std::vector<uint8_t*> outBuf{size_t(outCh)};
            for (int c = 0; c < outCh; ++c) {
                scratch[size_t(c)].resize(size_t(outSamples) * sizeof(float));
                outBuf[size_t(c)] = scratch[size_t(c)].data();
            }

            const int converted = swr_convert(swr, outBuf.data(), outSamples,
                                              const_cast<const uint8_t**>(fr->extended_data),
                                              fr->nb_samples);
            if (converted > 0) {
                for (int c = 0; c < outCh; ++c) {
                    const float* p = reinterpret_cast<const float*>(outBuf[size_t(c)]);
                    ch[size_t(c)].insert(ch[size_t(c)].end(), p, p + converted);
                }
            }
            av_frame_unref(fr);
        }
    }

    if (ch.empty() || ch[0].empty())
        return nullptr;

    decoded->channelData = std::move(ch);
    decoded->channelPtrs.reserve(size_t(outCh));
    for (int c = 0; c < outCh; ++c)
        decoded->channelPtrs.push_back(decoded->channelData[size_t(c)].data());
    return decoded;
}

#endif // YAVE_HAVE_FFMPEG

} // anonymous namespace

AudioSourceCache::AudioSourceCache(QObject* parent) : QObject(parent) {}

void AudioSourceCache::setProject(Project* project)
{
    project_ = project;
    cache_.clear();
}

void AudioSourceCache::clear()
{
    cache_.clear();
}

std::shared_ptr<audio::DecodedAudio> AudioSourceCache::decodeAsset(const QUuid& assetId)
{
    if (assetId.isNull() || !project_)
        return nullptr;

    const auto it = cache_.constFind(assetId);
    if (it != cache_.cend())
        return it.value();

    const Asset* asset = project_->assets()->asset(assetId);
    if (!asset || asset->isMissing || asset->resolvedAbsolutePath.isEmpty())
        return nullptr;

    std::shared_ptr<audio::DecodedAudio> decoded;
#if defined(YAVE_HAVE_FFMPEG)
    decoded = decodeFileToPcm(asset->resolvedAbsolutePath, project_->sampleRate());
#else
    Q_UNUSED(asset);
#endif
    if (decoded)
        cache_.insert(assetId, decoded);
    return decoded;
}

} // namespace yave::app