#pragma once

#include <yave/sdk/ParameterSchema.h>

#include <QUuid>
#include <QString>
#include <functional>
#include <memory>

namespace yave::sdk {
class ISubtitleEffect;
}

namespace yave::plugin {

/// 字幕エフェクトの所有ポインタ。
/// 外部プラグイン由来のインスタンスは、そのプラグインの destroyEffect で
/// 解放する必要があるため、deleter を型に含める。
using SubtitleEffectPtr =
    std::unique_ptr<yave::sdk::ISubtitleEffect,
                    std::function<void(yave::sdk::ISubtitleEffect*)>>;

} // namespace yave::plugin

namespace yave::subtitle {

/// 字幕クリップのエフェクトスタックに積まれる 1 要素。
struct SubtitleEffectInstance
{
    // ---- 永続化される ----
    QUuid     instanceId;
    QString   effectId;      ///< "yave.typewriter" / "com.example.glitch"
    QString   pluginId;      ///< 組み込みなら空文字列
    bool      enabled = true;
    yave::sdk::ParameterValues params;

    // ---- 実行時のみ (永続化しない) ----
    /// このインスタンス専用のエフェクト実装。
    /// prepare() の前計算結果を内部に持つため、クリップ間で共有しない。
    /// PluginManager::createSubtitleEffect() で生成する。
    plugin::SubtitleEffectPtr effect;
    bool     prepared = false;                  ///< prepare() 済みか
    bool     missing  = false;                 ///< プラグイン未インストール

    /// レイアウトが変わったら prepare() をやり直す必要がある。
    /// SubtitleClip::contentRevision() と比較して判定する。
    uint64_t preparedForRevision = 0;

    SubtitleEffectInstance() = default;
    SubtitleEffectInstance(SubtitleEffectInstance&&) = default;
    SubtitleEffectInstance& operator=(SubtitleEffectInstance&&) = default;

    /// 永続データのみを複製し、effect は新規生成する (クリップ複製時に使う)。
    SubtitleEffectInstance cloneForNewClip() const;

    static SubtitleEffectInstance create(const QString& effectId,
                                         const QString& pluginId = {});
};

} // namespace yave::subtitle
