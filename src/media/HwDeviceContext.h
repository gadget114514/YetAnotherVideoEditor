#pragma once

#include "VideoDecoder.h"

#include <QString>

namespace yave::media {

/// HW デバイス生成と優先順位決定 (4 章)。
///
/// 優先順位:
///   Windows : D3D11VA > NVDEC(CUDA) > QSV
///   macOS   : VideoToolbox
///
/// FFmpeg 無効ビルドでは createBest() が常に HwAccel::None を返す。
class HwDeviceContext
{
public:
    struct Device
    {
        HwAccel accel = HwAccel::None;
        void*   avBufferRef = nullptr;   ///< AVBufferRef* (FFmpeg 有効時)

        bool isHardware() const { return accel != HwAccel::None; }
    };

    /// コーデックに対して使える最良の HW デバイスを作る。
    /// 失敗したら None を返す (呼び出し側は SW デコードへフォールバック)。
    static Device createBest(int codecId);

    static const char* accelName(HwAccel a);
};

} // namespace yave::media
