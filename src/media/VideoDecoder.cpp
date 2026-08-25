#include "VideoDecoder.h"

#include <QThread>

// ===========================================================================
//  FFmpeg 有効ビルドの実装
//
//  open() の流れ (12.7 参照):
//    (1) コンテナを開く            avformat_open_input
//    (2) ストリーム情報取得        avformat_find_stream_info
//    (3) ビデオストリーム選択      av_find_best_stream
//    (4) HW デバイス設定           HwDeviceContext::createBest + get_format
//    (5) スレッド設定              thread_count / thread_type
//    (6) オープン                  avcodec_open2
//    (7) 尺の算出                  nb_frames もしくは duration
// ===========================================================================

#if defined(YAVE_HAVE_FFMPEG)

extern "C" {
#  include <libavcodec/avcodec.h>
#  include <libavformat/avformat.h>
#  include <libavutil/imgutils.h>
#  include <libavutil/opt.h>
#  include <libswresample/swresample.h>
#  include <libswscale/swscale.h>
}

#endif

#include <algorithm>

namespace yave::media {

#if defined(YAVE_HAVE_FFMPEG)

void AVFrameDeleter::operator()(AVFrame* p) const noexcept { av_frame_free(&p); }
void AVPacketDeleter::operator()(AVPacket* p) const noexcept { av_packet_free(&p); }
void AVCodecContextDeleter::operator()(AVCodecContext* p) const noexcept
{ avcodec_free_context(&p); }
void AVFormatContextDeleter::operator()(AVFormatContext* p) const noexcept
{
    if (p)
        avformat_close_input(&p);
}

FramePtr makeFrame()
{
    return FramePtr(av_frame_alloc());
}
PacketPtr makePacket()
{
    return PacketPtr(av_packet_alloc());
}

// SwsContext / SwrContext 用の簡易 deleter (この TU のみで使用)
struct SwsContextDeleter { void operator()(SwsContext* p) const noexcept { sws_freeContext(p); } };
using SwsPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;
struct SwrContextDeleter { void operator()(SwrContext* p) const noexcept { swr_free(&p); } };
using SwrPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

struct VideoDecoder::Impl
{
    FormatContextPtr fmt;
    CodecContextPtr  codec;
    int              streamIndex = -1;
    FramePtr         frame       = makeFrame();
    PacketPtr        packet      = makePacket();
    SwsPtr           sws;

    int64_t nextSourceFrame = 0;   ///< デコード済みフレームカウンタ

    // RGBA 変換用スクラッチ
    QByteArray rgbaBuffer;
};

VideoDecoder::VideoDecoder() : impl_(std::make_unique<Impl>()) {}
VideoDecoder::~VideoDecoder() = default;

bool VideoDecoder::open(const OpenParams& params, QString* errorOut)
{
    close();

    AVFormatContext* fmt = nullptr;
    if (avformat_open_input(&fmt, params.filePath.toUtf8().constData(),
                            nullptr, nullptr) < 0 || !fmt) {
        if (errorOut)
            *errorOut = QStringLiteral("avformat_open_input failed");
        return false;
    }
    impl_->fmt.reset(fmt);

    if (avformat_find_stream_info(impl_->fmt.get(), nullptr) < 0) {
        if (errorOut)
            *errorOut = QStringLiteral("avformat_find_stream_info failed");
        return false;
    }

    // ---- ビデオストリームとデコーダを選ぶ ----
    const AVCodec* decoder = nullptr;
    impl_->streamIndex =
        av_find_best_stream(impl_->fmt.get(), AVMEDIA_TYPE_VIDEO,
                            params.streamIndex, -1, &decoder, 0);
    if (impl_->streamIndex < 0 || !decoder) {
        if (errorOut)
            *errorOut = QStringLiteral("No video stream found.");
        return false;
    }

    AVStream* stream = impl_->fmt->streams[unsigned(impl_->streamIndex)];

    // ---- コーデックコンテキストを作る ----
    impl_->codec.reset(avcodec_alloc_context3(decoder));
    if (!impl_->codec) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to allocate codec context.");
        return false;
    }
    if (avcodec_parameters_to_context(impl_->codec.get(), stream->codecpar) < 0) {
        if (errorOut)
            *errorOut = QStringLiteral("avcodec_parameters_to_context failed");
        return false;
    }

    // ---- HW アクセラレーション ----
    // YAVE_HAVE_FFMPEG 環境では HwDeviceContext::createBest() を使う。
    // get_format コールバックで HW ピクセルフォーマットを選ばせる必要があるが、
    // 出力を SW RGBA へ落とす運用のため、まずは SW デコードへフォールバックする。
    // (D3D11VA/NVDEC 直結は yave_render の D3D11Interop と組み合わせて有効化)

    // ---- スレッド設定 ----
    impl_->codec->thread_count =
        params.threadCount > 0 ? params.threadCount
                               : std::max(1, QThread::idealThreadCount() / 2);
    impl_->codec->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

    // ---- オープン ----
    if (avcodec_open2(impl_->codec.get(), decoder, nullptr) < 0) {
        if (errorOut)
            *errorOut = QStringLiteral("avcodec_open2 failed");
        return false;
    }

    // ---- 尺の算出 ----
    info_.hasVideo   = true;
    info_.resolution = QSize(stream->codecpar->width, stream->codecpar->height);
    AVRational fr    = stream->avg_frame_rate.num != 0 ? stream->avg_frame_rate
                                                       : stream->r_frame_rate;
    info_.frameRateNum = fr.num;
    info_.frameRateDen = fr.den != 0 ? fr.den : 1;
    if (stream->nb_frames > 0)
        info_.durationFrames = int64_t(stream->nb_frames);
    else if (stream->duration != AV_NOPTS_VALUE && fr.den != 0)
        info_.durationFrames =
            int64_t(double(stream->duration) * double(fr.num) / double(fr.den));

    opened_ = true;
    return true;
}

void VideoDecoder::close()
{
    if (!impl_)
        return;
    impl_->codec.reset();
    impl_->fmt.reset();
    impl_->sws.reset();
    impl_->streamIndex = -1;
    impl_->nextSourceFrame = 0;
    opened_ = false;
}

bool VideoDecoder::seekToSourceFrame(int64_t sourceFrame)
{
    if (!opened_ || info_.frameRateDen == 0 || info_.frameRateNum == 0)
        return false;

    AVRational fr{int(info_.frameRateNum), int(info_.frameRateDen)};
    const int64_t ts = av_rescale_q(sourceFrame, av_inv_q(fr),
                                    impl_->fmt->streams[size_t(impl_->streamIndex)]->time_base);
    if (av_seek_frame(impl_->fmt.get(), impl_->streamIndex, ts,
                      AVSEEK_FLAG_BACKWARD) < 0)
        return false;
    avcodec_flush_buffers(impl_->codec.get());
    impl_->nextSourceFrame = sourceFrame;
    return true;
}

bool VideoDecoder::decodeNext(int64_t* sourceFrameOut, QByteArray* outPixels,
                              QSize* sizeOut)
{
    if (!opened_)
        return false;

    AVStream* stream = impl_->fmt->streams[size_t(impl_->streamIndex)];
    const int w = impl_->codec->width;
    const int h = impl_->codec->height;

    while (true) {
        if (av_read_frame(impl_->fmt.get(), impl_->packet.get()) < 0)
            return false;   ///< EOF

        if (impl_->packet->stream_index != impl_->streamIndex) {
            impl_->packet.reset(makePacket().release());   // 再利用のため新規確保
            continue;
        }

        if (avcodec_send_packet(impl_->codec.get(), impl_->packet.get()) < 0) {
            error_ = true;
            return false;
        }

        while (avcodec_receive_frame(impl_->codec.get(), impl_->frame.get()) == 0) {
            // RGBA8 premultiplied へ変換 (BGRA 出力の sws を RGBA として扱う)
            if (!impl_->sws) {
                impl_->sws.reset(sws_getContext(
                    w, h, AVPixelFormat(impl_->codec->pix_fmt), w, h,
                    AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr));
                if (!impl_->sws) {
                    error_ = true;
                    return false;
                }
            }

            outPixels->resize(size_t(w) * size_t(h) * 4);
            uint8_t* dst[4]     = {reinterpret_cast<uint8_t*>(outPixels->data()),
                                   nullptr, nullptr, nullptr};
            int      dstStride[4] = {w * 4, 0, 0, 0};
            sws_scale(impl_->sws.get(), impl_->frame->data, impl_->frame->linesize, 0, h,
                      dst, dstStride);

            // プレゼンテーション時刻からソース内フレーム番号を算出
            int64_t frameIdx = impl_->nextSourceFrame++;
            if (impl_->frame->best_effort_timestamp != AV_NOPTS_VALUE
                && info_.frameRateDen != 0 && info_.frameRateNum != 0) {
                frameIdx = av_rescale_q(
                    impl_->frame->best_effort_timestamp, stream->time_base,
                    AVRational{info_.frameRateDen, info_.frameRateNum});
            }

            if (sourceFrameOut)
                *sourceFrameOut = frameIdx;
            if (sizeOut)
                *sizeOut = QSize(w, h);

            av_frame_unref(impl_->frame.get());
            return true;
        }
    }
}

// ===========================================================================
//  AudioDecoder
// ===========================================================================

struct AudioDecoder::Impl
{
    // libswresample による 48kHz stereo float への正規化コンテキスト。
    FormatContextPtr fmt;
    CodecContextPtr  codec;
    SwrPtr           swr;

    int64_t nextFrame = 0;
};

AudioDecoder::AudioDecoder(const QString& filePath)
    : filePath_(filePath), impl_(std::make_unique<Impl>())
{}

AudioDecoder::~AudioDecoder() = default;

bool AudioDecoder::open(QString* errorOut)
{
    Q_UNUSED(errorOut);
#if defined(YAVE_HAVE_FFMPEG)
    // 実装: libavformat で開き、libswresample で 48kHz stereo float へ正規化する。
    // read() はインターリーブ float32 を出力する。
#endif
    return false;
}

int64_t AudioDecoder::read(float*, int64_t maxFrames)
{
    Q_UNUSED(maxFrames);
    return 0;
}

#else   // !YAVE_HAVE_FFMPEG

struct VideoDecoder::Impl {};
struct AudioDecoder::Impl {};

VideoDecoder::VideoDecoder() = default;
VideoDecoder::~VideoDecoder() = default;

bool VideoDecoder::open(const OpenParams&, QString* errorOut)
{
    if (errorOut)
        *errorOut = QStringLiteral("FFmpeg support is not enabled in this build.");
    return false;
}
void VideoDecoder::close() {}
bool VideoDecoder::seekToSourceFrame(int64_t) { return false; }
bool VideoDecoder::decodeNext(int64_t*, QByteArray*, QSize*) { return false; }

AudioDecoder::AudioDecoder(const QString&) : impl_(std::make_unique<Impl>()) {}
AudioDecoder::~AudioDecoder() = default;
bool AudioDecoder::open(QString* errorOut)
{
    if (errorOut)
        *errorOut = QStringLiteral("FFmpeg support is not enabled in this build.");
    return false;
}
int64_t AudioDecoder::read(float*, int64_t) { return 0; }

#endif


// (実装は上記。名前空間はここで閉じる)

} // namespace yave::media
