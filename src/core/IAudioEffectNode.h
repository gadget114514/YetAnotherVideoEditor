#pragma once

#include "Clip.h"

namespace yave {

/// 音声エフェクトノードの抽象インタフェース。
///
/// yave_core は yave_plugin (VST3 ホスト) に依存できないため、
/// Track のエフェクトチェーンはこのインタフェースのポインタを保持する。
/// 実際の VST3 処理は plugin::Vst3ProcessorNode がこのインタフェースを実装する。
class IAudioEffectNode
{
public:
    virtual ~IAudioEffectNode() = default;

    /// プラグインが申告する処理遅延 (サンプル数)。PDC 計算に使う。
    virtual int64_t latencySamples() const = 0;

    /// 有効か。bypass 中は false を返す。
    virtual bool isEnabled() const = 0;

    /// 永続化用の一意なネイティブ ID (VST3 class id 等)。空なら保存対象外。
    virtual QString nativeId() const = 0;

    /// リアルタイム音声処理。バッファを in-place で更新する。
    /// RT スレッド (noexcept)。malloc/free/mutex/Qt シグナル禁止。
    /// デフォルト実装はノーオーパー (VST3 が未接続の場合)。
    virtual void processRt(float* const* buffers, int channels, int frames) noexcept
    {
        Q_UNUSED(buffers);
        Q_UNUSED(channels);
        Q_UNUSED(frames);
    }
};

} // namespace yave
