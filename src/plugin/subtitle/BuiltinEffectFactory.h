#pragma once

#include "../PluginManager.h"
#include "../SubtitleEffectRegistry.h"

namespace yave::plugin {

/// 組み込み字幕エフェクトのファクトリ (6.6.4)。
/// Registry の初期化と PluginManager への接続を担う。
class BuiltinEffectFactory
{
public:
    /// 起動時に 1 回呼ぶ。組み込みエフェクトを利用可能にし、
    /// インスタンス生成ブリッジを接続する。
    static void install()
    {
        // BuiltinEffectRegistry::instance() への初回アクセスで
        // setEffectFactory ブリッジが接続される。
        (void)subtitle::BuiltinEffectRegistry::instance();
    }

    /// 利用可能な組み込みエフェクト id 一覧
    static QStringList builtinIds()
    {
        QStringList out;
        for (const auto& fx : subtitle::BuiltinEffectRegistry::instance().prototypes())
            out << fx->id();
        return out;
    }
};

} // namespace yave::plugin
