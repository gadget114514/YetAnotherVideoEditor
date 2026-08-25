#include "SubtitleEffectLoader.h"
#include "../SubtitleEffectRegistry.h"

#include "../../../include/yave/sdk/SubtitleEffectApi.h"
#include "../../util/Log.h"

#include <QLibrary>
#include <QCoreApplication>

#include <vector>

namespace yave::plugin {

std::vector<SubtitleEffectLoader::Loaded>& SubtitleEffectLoader::loaded()
{
    static std::vector<Loaded> s;
    return s;
}

bool SubtitleEffectLoader::load(const QString& path, SubtitleEffectRegistry& registry)
{
    auto lib = std::make_unique<QLibrary>(path);
    if (!lib->load()) {
        qCWarning(lcPlugin) << "load failed:" << path << lib->errorString();
        return false;
    }

    using FactoryFn = const yave::sdk::SubtitleEffectFactoryV1* (*)();
    auto fn = reinterpret_cast<FactoryFn>(lib->resolve("yaveCreateSubtitleEffectFactory"));
    if (!fn) {
        qCWarning(lcPlugin) << "entry point not found:" << path;
        return false;
    }

    const auto* factory = fn();
    if (!factory || factory->apiVersion != yave::sdk::kSubtitleEffectApiVersion) {
        qCWarning(lcPlugin) << "API version mismatch:" << path
                            << (factory ? factory->apiVersion : -1);
        lib->unload();
        return false;
    }

    // エフェクトのプロトタイプを取得して Registry へ登録する。
    // destroyEffect は Registry 側の所有ポインタ (deleter) が呼ぶ。
    std::vector<yave::sdk::ISubtitleEffect*> effects;
    for (int i = 0; i < factory->effectCount(); ++i) {
        if (auto* fx = factory->createEffect(i))
            effects.push_back(fx);
    }
    registry.registerExternal(factory->pluginId ? QString::fromLatin1(factory->pluginId)
                                                : path, effects);

    Loaded loadedEntry;
    loadedEntry.path    = path;
    loadedEntry.library = std::move(lib);
    loadedEntry.factory = factory;
    loaded().push_back(std::move(loadedEntry));

    // 同梱翻訳のロードは LanguageManager::registerPluginTranslations へ (10章)。
    // LanguageManager は app 層が所有するため、ここでは翻訳接頭辞を
    // ログへ出すのみ。app 層の起動シーケンスで再ロードされる。
    if (factory->translationQmPrefix) {
        qCInfo(lcPlugin) << "plugin provides translations:"
                         << factory->translationQmPrefix;
    }

    return true;
}

bool SubtitleEffectLoader::probe(const QString& path)
{
    QLibrary* lib = new QLibrary(path);
    if (!lib->load()) {
        delete lib;
        return false;
    }

    auto fn = reinterpret_cast<const yave::sdk::SubtitleEffectFactoryV1* (*)()>(
        lib->resolve("yaveCreateSubtitleEffectFactory"));
    if (!fn) {
        lib->unload();
        delete lib;
        return false;
    }
    const auto* factory = fn();
    const bool ok = factory && factory->apiVersion == yave::sdk::kSubtitleEffectApiVersion;
    // lib->unload(); // Do not unload to prevent destructor crash
    return ok;
}

void SubtitleEffectLoader::unloadAll()
{
    for (auto& entry : loaded()) {
        if (entry.factory && entry.factory->shutdown)
            entry.factory->shutdown();
        if (entry.library)
            entry.library->unload();
    }
    loaded().clear();
}

} // namespace yave::plugin
