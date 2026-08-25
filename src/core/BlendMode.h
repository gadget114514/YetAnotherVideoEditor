#pragma once

namespace yave {

/// ブレンドモード。シェーダの uniform 分岐と同じ値を持つ (layer_blend.frag 参照)。
enum class BlendMode : int
{
    Normal = 0, Add, Multiply, Screen, Overlay, Darken, Lighten,
    ColorDodge, ColorBurn, Difference, Exclusion, AlphaMask
};

} // namespace yave
