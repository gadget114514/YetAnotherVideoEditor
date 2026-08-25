#include "SubtitleEffectInstance.h"
#include "SubtitleEffectFactoryBridge.h"

#include <yave/sdk/ISubtitleEffect.h>

namespace yave::subtitle {

SubtitleEffectInstance SubtitleEffectInstance::cloneForNewClip() const
{
    SubtitleEffectInstance out;
    out.instanceId = QUuid::createUuid();
    out.effectId   = effectId;
    out.pluginId   = pluginId;
    out.enabled    = enabled;
    out.params     = params;

    // effect は新規生成する (prepare() の前計算結果はインスタンス固有のため共有不可)。
    out.effect  = createEffectViaFactory(effectId);
    out.missing = (out.effect == nullptr);
    return out;
}

SubtitleEffectInstance SubtitleEffectInstance::create(const QString& effectId,
                                                      const QString& pluginId)
{
    SubtitleEffectInstance inst;
    inst.instanceId = QUuid::createUuid();
    inst.effectId   = effectId;
    inst.pluginId   = pluginId;

    inst.effect  = createEffectViaFactory(effectId);
    inst.missing = (inst.effect == nullptr);
    if (!inst.missing) {
        // スキーマの既定値で params を埋める
        for (const auto& def : inst.effect->parameterSchema())
            if (def.defaultValue.isValid())
                inst.params.set(def.key, def.defaultValue);
    }
    return inst;
}

} // namespace yave::subtitle
