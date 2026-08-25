#include "RenderSnapshot.h"

namespace yave {

// RenderSnapshot は純粋なデータ構造 (POD コンテナ) である。
// 実装を持つのは QMatrix4x4 / QRectF の暗黙共有を正しく機能させるための
// デストラクタ定義のみ。ヘッダオンリーにすると全 TU で仮想テーブルが
// 発生するわけではないが、明示的に cpp を分けてコンパイルコストを制御する。

static_assert(std::is_copy_constructible_v<RenderSnapshot>,
              "RenderSnapshot must be copyable (UI -> Render thread handoff)");
static_assert(std::is_move_constructible_v<RenderSnapshot>,
              "RenderSnapshot must be movable");

} // namespace yave
