#include "MediaProbe.h"

#if defined(YAVE_HAVE_FFMPEG)
extern "C" {
#  include <libavformat/avformat.h>
#  include <libavutil/error.h>
}
#endif

namespace yave::media {

MediaInfo MediaProbe::probe(const QString& filePath)
{
    MediaInfo info;

#if defined(YAVE_HAVE_FFMPEG)
    AVFormatContext* fmt = nullptr;
    const QByteArray pathUtf8 = filePath.toUtf8();
    if (avformat_open_input(&fmt, pathUtf8.constData(), nullptr, nullptr) < 0 || !fmt) {
        info.error = QStringLiteral("avformat_open_input failed");
        return info;
    }
    struct FormatGuard
    {
        ~FormatGuard() { if (ctx) avformat_close_input(&ctx); }
        AVFormatContext* ctx;
    } guard{fmt};

    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        info.error = QStringLiteral("avformat_find_stream_info failed");
        return info;
    }

    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        const AVStream* s = fmt->streams[i];
        if (!s)
            continue;
        if (s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && !info.hasVideo) {
            info.hasVideo   = true;
            info.resolution = QSize(s->codecpar->width, s->codecpar->height);
            AVRational fr   = s->avg_frame_rate.num != 0 ? s->avg_frame_rate
                                                         : s->r_frame_rate;
            info.frameRateNum = fr.num;
            info.frameRateDen = fr.den != 0 ? fr.den : 1;
            if (s->nb_frames > 0)
                info.durationFrames = int64_t(s->nb_frames);
            else if (s->duration != AV_NOPTS_VALUE && fr.den != 0)
                info.durationFrames = int64_t(double(s->duration) * double(fr.num)
                                              / double(fr.den));
        } else if (s->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && !info.hasAudio) {
            info.hasAudio       = true;
            info.audioSampleRate = s->codecpar->sample_rate;
            info.audioChannels   = s->codecpar->ch_layout.nb_channels;
            if (s->duration != AV_NOPTS_VALUE && s->codecpar->sample_rate > 0)
                info.audioDurationFrames =
                    int64_t(double(s->duration) * s->codecpar->sample_rate);
        }
    }

    info.ok = true;
#else
    Q_UNUSED(filePath);
    info.error = QStringLiteral(
        "FFmpeg support is not enabled in this build. "
        "Rebuild with YAVE_ENABLE_FFMPEG=ON and FFmpeg installed.");
#endif

    return info;
}

} // namespace yave::media
