#include "HwDeviceContext.h"

#include <QList>

#if defined(YAVE_HAVE_FFMPEG)
extern "C" {
#  include <libavcodec/avcodec.h>
#  include <libavutil/hwcontext.h>
}
#endif

namespace yave::media {

const char* HwDeviceContext::accelName(HwAccel a)
{
    switch (a) {
    case HwAccel::None:         return "none";
    case HwAccel::D3D11VA:      return "d3d11va";
    case HwAccel::Nvdec:        return "nvdec";
    case HwAccel::Qsv:          return "qsv";
    case HwAccel::VideoToolbox: return "videotoolbox";
    }
    return "unknown";
}

#if defined(YAVE_HAVE_FFMPEG)

namespace {

AVHWDeviceType accelToType(HwAccel a)
{
    switch (a) {
    case HwAccel::D3D11VA:      return AV_HWDEVICE_TYPE_D3D11VA;
    case HwAccel::Nvdec:        return AV_HWDEVICE_TYPE_CUDA;
    case HwAccel::Qsv:          return AV_HWDEVICE_TYPE_QSV;
    case HwAccel::VideoToolbox: return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
    default:                    return AV_HWDEVICE_TYPE_NONE;
    }
}

/// プラットフォームごとの優先順位で候補を並べる
QList<HwAccel> candidateOrder()
{
#if defined(_WIN32)
    return { HwAccel::D3D11VA, HwAccel::Nvdec, HwAccel::Qsv };
#elif defined(__APPLE__)
    return { HwAccel::VideoToolbox };
#else
    return { HwAccel::Nvdec, HwAccel::Qsv, HwAccel::D3D11VA };
#endif
}

} // anonymous namespace

HwDeviceContext::Device HwDeviceContext::createBest(int codecId)
{
    Q_UNUSED(codecId);

    for (HwAccel accel : candidateOrder()) {
        const AVHWDeviceType type = accelToType(accel);
        if (type == AV_HWDEVICE_TYPE_NONE)
            continue;

        AVBufferRef* ref = nullptr;
        if (av_hwdevice_ctx_create(&ref, type, nullptr, nullptr, 0) == 0 && ref) {
            Device dev;
            dev.accel      = accel;
            dev.avBufferRef = ref;   ///< 呼び出し側が av_buffer_unref する
            return dev;
        }
    }

    return {};
}

#else

HwDeviceContext::Device HwDeviceContext::createBest(int)
{
    return {};   ///< FFmpeg 無効ビルドでは常に None
}

#endif

} // namespace yave::media
