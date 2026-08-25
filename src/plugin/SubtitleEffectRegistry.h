#pragma once

#include "../subtitle/SubtitleEffectFactoryBridge.h"
#include "../subtitle/SubtitleEffectInstance.h"

#include <yave/sdk/ISubtitleEffect.h>

#include <QHash>
#include <QString>

#include <memory>
#include <vector>

namespace yave::subtitle {

/// 組み込みエフェクトのプロトタイプ管理。
///
/// Registry は**プロトタイプ**を 1 個だけ保持する (一覧表示と parameterSchema()
/// の取得に使う)。実際にクリップへ積むときは createInstance() で
/// **新しいインスタンスを生成**する。
class BuiltinEffectRegistry
{
public:
    static BuiltinEffectRegistry& instance();

    /// 全組み込みエフェクトのプロトタイプ一覧
    const std::vector<std::unique_ptr<yave::sdk::ISubtitleEffect>>& prototypes() const;

    /// id で検索。無ければ nullptr。
    const yave::sdk::ISubtitleEffect* find(const QString& effectId) const;

private:
    BuiltinEffectRegistry();
    std::vector<std::unique_ptr<yave::sdk::ISubtitleEffect>> builtins_;
};

} // namespace yave::subtitle

namespace yave::plugin {

class SubtitleEffectLoader;

/// 字幕エフェクトの統合レジストリ。
/// 組み込みエフェクト + 外部プラグイン (.dll/.dylib) を id -> プロトタイプで引く。
class SubtitleEffectRegistry
{
public:
    SubtitleEffectRegistry();

    void registerExternal(const QString& pluginId,
                          std::vector<yave::sdk::ISubtitleEffect*> effects);

    /// effectId でプロトタイプを引く。組み込みを先に見る。無ければ nullptr。
    const yave::sdk::ISubtitleEffect* prototype(const QString& effectId) const;

    /// 新しいインスタンスを生成する。所有は呼び出し側。
    /// 外部プラグイン由来の場合、deleter が destroyEffect を呼ぶ。
    SubtitleEffectPtr createInstance(const QString& effectId) const;

    bool isKnown(const QString& effectId) const { return prototype(effectId) != nullptr; }

private:
    struct ExternalEntry
    {
        QString pluginId;
        std::vector<yave::sdk::ISubtitleEffect*> effects;
        std::vector<SubtitleEffectPtr> owned;   ///< プロトタイプの所有権
    };

    subtitle::BuiltinEffectRegistry* builtin_ = nullptr;
    std::vector<ExternalEntry>       externals_;
};

} // namespace yave::plugin
