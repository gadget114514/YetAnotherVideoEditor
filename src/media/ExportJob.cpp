#include "ExportJob.h"

#include <atomic>

#if defined(YAVE_HAVE_FFMPEG)
extern "C" {
#  include <libavcodec/avcodec.h>
#  include <libavformat/avformat.h>
#  include <libavutil/opt.h>
#  include <libswscale/swscale.h>
}
#endif

namespace yave::media {

ExportJob::ExportJob(ExportSettings settings)
    : settings_(std::move(settings))
{}

QString ExportJob::run(ExportProgressFn progress, void* userData)
{
    if (settings_.outputPath.isEmpty())
        return QStringLiteral("Output path is empty.");
    if (settings_.endFrame <= settings_.startFrame)
        return QStringLiteral("Empty export range.");

#if defined(YAVE_HAVE_FFMPEG)
    // ---- Muxer を開く ----
    AVFormatContext* fmt = nullptr;
    if (avformat_alloc_output_context2(&fmt, nullptr, nullptr,
                                       settings_.outputPath.toUtf8().constData()) < 0
        || !fmt) {
        return QStringLiteral("avformat_alloc_output_context2 failed");
    }

    const AVCodec* vcodec = avcodec_find_encoder_by_name(
        settings_.videoCodec.toUtf8().constData());
    if (!vcodec)
        vcodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!vcodec) {
        avformat_free_context(fmt);
        return QStringLiteral("H.264 encoder not found");
    }

    AVStream* vs = avformat_new_stream(fmt, nullptr);
    if (!vs) {
        avformat_free_context(fmt);
        return QStringLiteral("avformat_new_stream failed");
    }
    vs->id = 0;

    AVCodecContext* c = avcodec_alloc_context3(vcodec);
    c->width     = settings_.resolution.width();
    c->height    = settings_.resolution.height();
    c->time_base = AVRational{int(settings_.frameRate.den), int(settings_.frameRate.num)};
    c->framerate = AVRational{int(settings_.frameRate.num), int(settings_.frameRate.den)};
    c->pix_fmt   = AV_PIX_FMT_YUV420P;
    c->bit_rate  = int64_t(settings_.videoBitrateKbps) * 1000;
    if (fmt->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(c, vcodec, nullptr) < 0) {
        avcodec_free_context(&c);
        avformat_free_context(fmt);
        return QStringLiteral("avcodec_open2 failed");
    }
    avcodec_parameters_from_context(vs->codecpar, c);
    vs->time_base = c->time_base;

    if (!(fmt->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt->pb, settings_.outputPath.toUtf8().constData(),
                      AVIO_FLAG_WRITE) < 0) {
            avcodec_free_context(&c);
            avformat_free_context(fmt);
            return QStringLiteral("avio_open failed");
        }
    }
    if (avformat_write_header(fmt, nullptr) < 0) {
        avcodec_free_context(&c);
        avformat_free_context(fmt);
        return QStringLiteral("avformat_write_header failed");
    }

    // ---- フレームループ ----
    const int64_t totalFrames = settings_.endFrame - settings_.startFrame;
    const int w = c->width;
    const int h = c->height;

    // RGBA -> YUV420P 変換用コンテキスト
    SwsContext* sws = sws_getContext(
        w, h, AV_PIX_FMT_BGRA, w, h, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws) {
        avcodec_free_context(&c);
        avformat_free_context(fmt);
        return QStringLiteral("sws_getContext failed");
    }

    AVFrame* yuvFrame = av_frame_alloc();
    yuvFrame->format = AV_PIX_FMT_YUV420P;
    yuvFrame->width  = w;
    yuvFrame->height = h;
    av_frame_get_buffer(yuvFrame, 0);

    AVPacket* pkt = av_packet_alloc();

    for (int64_t i = 0; i < totalFrames && !cancelled_; ++i) {
        // ピクセルデータを供給コールバックから取得
        QByteArray rgbaPixels;
        if (pixelProvider_) {
            rgbaPixels = pixelProvider_(settings_.startFrame + i, w, h);
        }

        if (!rgbaPixels.isEmpty()) {
            // RGBA -> YUV420P 変換
            const uint8_t* srcData[1] = {
                reinterpret_cast<const uint8_t*>(rgbaPixels.constData())
            };
            int srcStride[1] = { w * 4 };

            av_frame_make_writable(yuvFrame);
            sws_scale(sws, srcData, srcStride, 0, h,
                      yuvFrame->data, yuvFrame->linesize);

            yuvFrame->pts = i;

            if (avcodec_send_frame(c, yuvFrame) < 0) {
                cancelled_ = true;
                break;
            }

            while (avcodec_receive_packet(c, pkt) == 0) {
                pkt->stream_index = vs->index;
                av_packet_rescale_ts(pkt, c->time_base, vs->time_base);
                if (av_interleaved_write_frame(fmt, pkt) < 0) {
                    cancelled_ = true;
                    break;
                }
                av_packet_unref(pkt);
            }
        }

        if (progress && (i % 60 == 0)) {
            if (!progress(double(i) / double(totalFrames), userData))
                cancelled_ = true;
        }
    }

    // フラッシュ
    if (!cancelled_) {
        avcodec_send_frame(c, nullptr);
        while (avcodec_receive_packet(c, pkt) == 0) {
            pkt->stream_index = vs->index;
            av_packet_rescale_ts(pkt, c->time_base, vs->time_base);
            av_interleaved_write_frame(fmt, pkt);
            av_packet_unref(pkt);
        }
    }

    av_packet_free(&pkt);
    av_frame_free(&yuvFrame);
    sws_freeContext(sws);

    av_write_trailer(fmt);
    if (!(fmt->oformat->flags & AVFMT_NOFILE))
        avio_closep(&fmt->pb);
    avcodec_free_context(&c);
    avformat_free_context(fmt);

    return cancelled_ ? QStringLiteral("cancelled") : QString();
#else
    Q_UNUSED(progress);
    Q_UNUSED(userData);
    return QStringLiteral("FFmpeg support is not enabled in this build.");
#endif
}

} // namespace yave::media
