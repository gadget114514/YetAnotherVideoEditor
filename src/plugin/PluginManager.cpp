#include "PluginManager.h"
#include "subtitle/SubtitleEffectLoader.h"
#include "SubtitleEffectRegistry.h"

#include "../util/Log.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QThread>

#include <algorithm>

#if defined(YAVE_ENABLE_AVIUTL)
#  include "aviutl/AviUtlRegistry.h"
#endif

namespace yave::plugin {

// ===========================================================================
//  ライフサイクル
// ===========================================================================

PluginManager& PluginManager::instance()
{
    static PluginManager s;
    return s;
}

PluginManager::PluginManager()
    : QObject(nullptr)
{
    loadCache();

    // 組み込み + 外部字幕エフェクトのレジストリ
    subtitleFx_ = new SubtitleEffectRegistry();

    // ヘッダに #ifdef を書かないための設計 (12.1.2):
    // AviUtl レジストリは Windows 専用ビルドでのみ実体化する。
#if defined(YAVE_ENABLE_AVIUTL)
    aviutl_ = new AviUtlRegistry();
#endif
}

PluginManager::~PluginManager()
{
    unloadAll();
}

bool PluginManager::isAviUtlSupported()
{
#if defined(YAVE_ENABLE_AVIUTL)
    return true;
#else
    return false;
#endif
}

// ===========================================================================
//  走査
// ===========================================================================

void PluginManager::scanAsync()
{
    emit scanStarted();
    scanSync();
    emit scanFinished();
}

void PluginManager::scanSync()
{
    QMutexLocker lock(&mutex_);

    // ---- 字幕エフェクト: 組み込みは常に利用可能 ----
    (void)subtitleEffectPrototype(QStringLiteral("yave.fade"));

    // ---- 外部字幕エフェクト (.dll/.dylib) の走査 ----
    for (const QString& dir : searchPaths(PluginKind::SubtitleEffect)) {
        const QDir d(dir);
        for (const QFileInfo& fi :
             d.entryInfoList(QStringList{QStringLiteral("*.dll"),
                                         QStringLiteral("*.dylib"),
                                         QStringLiteral("*.so")}, QDir::Files)) {
            if (isBlacklisted(fi.absoluteFilePath()))
                continue;
            if (!SubtitleEffectLoader::probe(fi.absoluteFilePath())) {
                blacklist(fi.absoluteFilePath(),
                          QStringLiteral("invalid factory"));
            }
        }
    }

#if defined(YAVE_ENABLE_AVIUTL)
    if (aviutl_) {
        for (const QString& dir : searchPaths(PluginKind::AviUtlFilter))
            aviutl_->scanFilters(dir);
    }
#endif

    qCInfo(lcPlugin) << "Plugin scan finished";
}

bool PluginManager::isScanning() const
{
    return false;   ///< 現行実装は同期走査のため常に false
}

// ===========================================================================
//  一覧 / 検索
// ===========================================================================

std::vector<PluginDescriptor> PluginManager::plugins(PluginKind kind) const
{
    QMutexLocker lock(&mutex_);
    switch (kind) {
    case PluginKind::Vst3:
        break;
    case PluginKind::SubtitleEffect:
        break;
    case PluginKind::AviUtlFilter:
    case PluginKind::AviUtlInput:
#if defined(YAVE_ENABLE_AVIUTL)
        return aviutl_ ? aviutl_->descriptors(kind) : std::vector<PluginDescriptor>{};
#else
        return {};      ///< macOS では常に空。UI 側は特別な分岐を書かなくてよい
#endif
    }
    return {};
}

std::vector<PluginDescriptor> PluginManager::allPlugins() const
{
    std::vector<PluginDescriptor> out;
    for (PluginKind kind :
         {PluginKind::Vst3, PluginKind::SubtitleEffect,
          PluginKind::AviUtlFilter, PluginKind::AviUtlInput}) {
        auto list = plugins(kind);
        out.insert(out.end(), list.begin(), list.end());
    }
    return out;
}

const PluginDescriptor* PluginManager::find(const QUuid& uid) const
{
    const auto all = allPlugins();
    for (const auto& d : all)
        if (d.uid == uid)
            return &d;
    return nullptr;
}

const PluginDescriptor* PluginManager::findByNativeId(PluginKind kind,
                                                      const QString& nativeId) const
{
    for (const auto& d : plugins(kind))
        if (d.nativeId == nativeId)
            return &d;
    return nullptr;
}

// ===========================================================================
//  検索パス
// ===========================================================================

QStringList PluginManager::defaultSearchPaths(PluginKind kind)
{
    QStringList paths;
    switch (kind) {
    case PluginKind::Vst3:
#if defined(Q_OS_WIN)
        paths << QStringLiteral("C:/Program Files/Common Files/VST3");
#elif defined(Q_OS_MACOS)
        paths << QStringLiteral("/Library/Audio/Plug-Ins/VST3")
              << QStringLiteral("%1/Library/Audio/Plug-Ins/VST3")
                     .arg(QDir::homePath());
#endif
        break;
    case PluginKind::SubtitleEffect:
        paths << QCoreApplication::applicationDirPath() + QStringLiteral("/plugins");
        break;
    case PluginKind::AviUtlFilter:
    case PluginKind::AviUtlInput:
#if defined(Q_OS_WIN)
        paths << QStringLiteral("C:/Program Files/aviutl/plugins");
#endif
        break;
    }
    return paths;
}

QStringList PluginManager::searchPaths(PluginKind kind) const
{
    QMutexLocker lock(&mutex_);
    QSettings settings;
    return settings.value(QStringLiteral("plugins/searchPaths/%1").arg(int(kind)),
                          defaultSearchPaths(kind)).toStringList();
}

void PluginManager::setSearchPaths(PluginKind kind, const QStringList& paths)
{
    QMutexLocker lock(&mutex_);
    QSettings settings;
    settings.setValue(QStringLiteral("plugins/searchPaths/%1").arg(int(kind)), paths);
}

void PluginManager::addSearchPath(PluginKind kind, const QString& path)
{
    QMutexLocker lock(&mutex_);
    auto paths = searchPaths(kind);
    if (!paths.contains(path)) {
        paths.append(path);
        setSearchPaths(kind, paths);
    }
}

// ===========================================================================
//  インスタンス生成
// ===========================================================================

std::unique_ptr<Vst3Host> PluginManager::createVst3(const QUuid& uid,
                                                    double sampleRate,
                                                    int maxBlockSize,
                                                    QString* errorOut)
{
#if defined(YAVE_ENABLE_VST3)
    return Vst3HostFactory::create(uid, sampleRate, maxBlockSize, errorOut);
#else
    Q_UNUSED(uid);
    Q_UNUSED(sampleRate);
    Q_UNUSED(maxBlockSize);
    if (errorOut)
        *errorOut = QStringLiteral(
            "VST3 hosting is not enabled in this build "
            "(configure with YAVE_ENABLE_VST3=ON).");
    return nullptr;
#endif
}

const yave::sdk::ISubtitleEffect* PluginManager::subtitleEffectPrototype(
    const QString& effectId) const
{
    QMutexLocker lock(&mutex_);
    return subtitleFx_->prototype(effectId);
}

SubtitleEffectPtr PluginManager::createSubtitleEffect(const QString& effectId) const
{
    QMutexLocker lock(&mutex_);
    return subtitleFx_->createInstance(effectId);
}

bool PluginManager::isSubtitleEffectMissing(const QString& effectId) const
{
    QMutexLocker lock(&mutex_);
    return subtitleFx_->prototype(effectId) == nullptr;
}

// ===========================================================================
//  ブラックリスト
// ===========================================================================

void PluginManager::blacklist(const QString& filePath, const QString& reason)
{
    QMutexLocker lock(&mutex_);
    blacklist_.push_back({filePath, reason});
    saveCache();
}

void PluginManager::removeFromBlacklist(const QString& filePath)
{
    QMutexLocker lock(&mutex_);
    blacklist_.erase(std::remove_if(blacklist_.begin(), blacklist_.end(),
                                    [&](const BlacklistEntry& e) {
                                        return e.path == filePath;
                                    }),
                     blacklist_.end());
    saveCache();
}

void PluginManager::clearBlacklist()
{
    QMutexLocker lock(&mutex_);
    blacklist_.clear();
    saveCache();
}

bool PluginManager::isBlacklisted(const QString& filePath) const
{
    QMutexLocker lock(&mutex_);
    for (const auto& e : blacklist_)
        if (e.path == filePath)
            return true;
    return false;
}

std::vector<std::pair<QString, QString>> PluginManager::blacklistEntries() const
{
    QMutexLocker lock(&mutex_);
    std::vector<std::pair<QString, QString>> out;
    for (const auto& e : blacklist_)
        out.emplace_back(e.path, e.reason);
    return out;
}

// ===========================================================================
//  キャッシュ
// ===========================================================================

void PluginManager::loadCache()
{
    QMutexLocker lock(&mutex_);
    QSettings settings;
    const int n = settings.beginReadArray(QStringLiteral("plugins/blacklist"));
    for (int i = 0; i < n; ++i) {
        settings.setArrayIndex(i);
        BlacklistEntry e;
        e.path   = settings.value(QStringLiteral("path")).toString();
        e.reason = settings.value(QStringLiteral("reason")).toString();
        if (!e.path.isEmpty())
            blacklist_.push_back(std::move(e));
    }
    settings.endArray();
}

void PluginManager::saveCache() const
{
    QSettings settings;
    settings.beginWriteArray(QStringLiteral("plugins/blacklist"));
    int i = 0;
    for (const auto& e : blacklist_) {
        settings.setArrayIndex(i++);
        settings.setValue(QStringLiteral("path"), e.path);
        settings.setValue(QStringLiteral("reason"), e.reason);
    }
    settings.endArray();
}

void PluginManager::unloadAll()
{
    // シャットダウン順序 1.6: オーディオ RT スレッド停止後に呼ばれること。
    // VST3 -> terminate(), AviUtl -> FreeLibrary。
}

} // namespace yave::plugin
