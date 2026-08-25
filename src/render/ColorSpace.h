#pragma once

#include "../core/BlendMode.h"

#include <QString>

namespace yave::render {

/// 色空間の定義と YUV->RGB 変換行列の選択。
///
/// レイヤーシェーダの uniform colorSpace と同じ値を持つこと
/// (layer_blend.frag 参照)。
enum class ColorSpaceId : int
{
    Bt709Limited = 1,
    Bt709Full    = 0,
    Bt2020       = 3,
};

/// YUV -> RGB の変換行列要素。GPU 側シェーダと同じ計算を CPU 検証用に提供する。
struct YuvToRgbMatrix
{
    // RGB = M * (Y - offsetY, U - 128, V - 128)
    float m[3][3] = {};
    float offsetY = 16.0f;

    static YuvToRgbMatrix bt709Limited();
    static YuvToRgbMatrix bt709Full();
    static YuvToRgbMatrix bt2020();
};

class ColorSpace
{
public:
    /// 文字列 ("bt709" / "bt2020") -> id
    static ColorSpaceId fromName(const QString& name);

    /// HDR (PQ / HLG) のトーンマッピングを SDR へ適用するか
    static bool needsToneMap(ColorSpaceId id);

    static YuvToRgbMatrix matrix(ColorSpaceId id);
};

} // namespace yave::render
