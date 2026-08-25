#pragma once

#include "SubtitleEffectInstance.h"

#include <functional>

namespace yave::subtitle {

/// エフェクトインスタンス生成の注入ポイント。
///
/// yave_subtitle は yave_plugin に依存できないため、
/// 実際のインスタンス生成 (PluginManager / 組み込みファクトリへの委譲) は
/// 起動時にこのブリッジへ関数を登録することで行う。
/// 未登録の場合は nullptr (missing 扱い) を返す。
using EffectFactoryFn = std::function<plugin::SubtitleEffectPtr(const QString& effectId)>;

void setEffectFactory(EffectFactoryFn fn);
EffectFactoryFn effectFactory();

inline plugin::SubtitleEffectPtr createEffectViaFactory(const QString& effectId)
{
    auto fn = effectFactory();
    return fn ? fn(effectId) : nullptr;
}

} // namespace yave::subtitle
