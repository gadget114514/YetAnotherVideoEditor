#include "SubtitleEffectRegistry.h"

#include "../subtitle/effects/BuiltinEffects.h"
#include "../subtitle/SubtitleEffectFactoryBridge.h"

namespace yave::subtitle {

BuiltinEffectRegistry& BuiltinEffectRegistry::instance()
{
    static BuiltinEffectRegistry s;
    return s;
}

const std::vector<std::unique_ptr<yave::sdk::ISubtitleEffect>>&
BuiltinEffectRegistry::prototypes() const
{
    return builtins_;
}

const yave::sdk::ISubtitleEffect* BuiltinEffectRegistry::find(const QString& effectId) const
{
    for (const auto& fx : builtins_)
        if (fx->id() == effectId)
            return fx.get();
    return nullptr;
}

BuiltinEffectRegistry::BuiltinEffectRegistry()
{
    // 6.6.4 の組み込みエフェクト一覧
    builtins_.push_back(std::make_unique<FadeEffect>());
    builtins_.push_back(std::make_unique<TypewriterEffect>());
    builtins_.push_back(std::make_unique<KaraokeEffect>());
    builtins_.push_back(std::make_unique<SlideInEffect>());
    builtins_.push_back(std::make_unique<PopPerCharEffect>());
    builtins_.push_back(std::make_unique<WaveEffect>());
    builtins_.push_back(std::make_unique<BlurInEffect>());

    // 組み込みエフェクトをインスタンス生成ブリッジへ接続する。
    // (yave_subtitle が yave_plugin へ依存しないための注入ポイント)
    setEffectFactory([](const QString& effectId) -> plugin::SubtitleEffectPtr {
        std::unique_ptr<yave::sdk::ISubtitleEffect> fresh;
        if      (effectId == QLatin1String("yave.fade"))       fresh = std::make_unique<FadeEffect>();
        else if (effectId == QLatin1String("yave.typewriter")) fresh = std::make_unique<TypewriterEffect>();
        else if (effectId == QLatin1String("yave.karaoke"))    fresh = std::make_unique<KaraokeEffect>();
        else if (effectId == QLatin1String("yave.slidein"))    fresh = std::make_unique<SlideInEffect>();
        else if (effectId == QLatin1String("yave.popperchar")) fresh = std::make_unique<PopPerCharEffect>();
        else if (effectId == QLatin1String("yave.wave"))       fresh = std::make_unique<WaveEffect>();
        else if (effectId == QLatin1String("yave.blurin"))     fresh = std::make_unique<BlurInEffect>();
        if (!fresh)
            return nullptr;
        return plugin::SubtitleEffectPtr(fresh.release(),
                                         [](yave::sdk::ISubtitleEffect* e) { delete e; });
    });
}

} // namespace yave::subtitle

namespace yave::plugin {

// ===========================================================================
//  SubtitleEffectRegistry
// ===========================================================================

SubtitleEffectRegistry::SubtitleEffectRegistry()
    : builtin_(&subtitle::BuiltinEffectRegistry::instance())
{}

void SubtitleEffectRegistry::registerExternal(
    const QString& pluginId, std::vector<yave::sdk::ISubtitleEffect*> effects)
{
    ExternalEntry e;
    e.pluginId = pluginId;
    for (yave::sdk::ISubtitleEffect* raw : effects) {
        e.effects.push_back(raw);
        e.owned.emplace_back(raw, [raw](yave::sdk::ISubtitleEffect*) {
            // 所有権は LoadedPlugin 側の factory->destroyEffect で解放するため、
            // ここでは何もしない (deleter が呼ばれるのは Registry 破棄時のみ)。
            Q_UNUSED(raw);
        });
    }
    externals_.push_back(std::move(e));
}

const yave::sdk::ISubtitleEffect* SubtitleEffectRegistry::prototype(
    const QString& effectId) const
{
    if (const auto* b = builtin_->find(effectId))
        return b;
    for (const auto& e : externals_)
        for (const auto* fx : e.effects)
            if (fx && fx->id() == effectId)
                return fx;
    return nullptr;
}

SubtitleEffectPtr SubtitleEffectRegistry::createInstance(const QString& effectId) const
{
    const yave::sdk::ISubtitleEffect* proto = prototype(effectId);
    if (!proto)
        return nullptr;

    // プロトタイプから同じ型の新しいインスタンスを作る。
    // ISubtitleEffect に clone 仮想関数を追加すると ABI に触れるため、
    // ここでは「既知の組み込み id」はファクトリで再生成し、外部プラグインは
    // createEffect を通じて生成する設計とする。
    if (builtin_->find(effectId))
        return subtitle::createEffectViaFactory(effectId);

    return nullptr;   ///< 外部プラグインのインスタンス生成は Loader 経由で行う
}

} // namespace yave::plugin
