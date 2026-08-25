#pragma once

#include "PluginDescriptor.h"
#include "vst3/Vst3HostStub.h"

#include <QObject>
#include <QMutex>
#include <QStringList>
#include <QUuid>

#include <functional>
#include <memory>
#include <vector>

namespace yave::sdk {
class ISubtitleEffect;
}

namespace yave {

class Track;

} // namespace yave

namespace yave::plugin {

class Vst3Registry;
class SubtitleEffectRegistry;
class AviUtlRegistry;          ///< Windows 以外では定義されない (前方宣言のみ)

/// 字幕エフェクトの所有ポインタ。
/// 外部プラグイン由来のインスタンスは、そのプラグインの destroyEffect で
/// 解放する必要があるため、deleter を型に含める。
using SubtitleEffectPtr =
    std::unique_ptr<yave::sdk::ISubtitleEffect,
                    std::function<void(yave::sdk::ISubtitleEffect*)>>;

class PluginManager : public QObject
{
    Q_OBJECT
public:
    static PluginManager& instance();

    // ================= 走査 =================

    /// バックグラウンドで走査を開始する。起動時に一度呼ぶ。
    /// plugin_cache.json のキャッシュが有効なファイルは再走査しない。
    void scanAsync();
    void scanSync();                    ///< テスト用
    bool isScanning() const;

    // ================= 一覧 =================

    std::vector<PluginDescriptor> plugins(PluginKind kind) const;
    std::vector<PluginDescriptor> allPlugins() const;

    const PluginDescriptor* find(const QUuid& uid) const;
    const PluginDescriptor* findByNativeId(PluginKind kind, const QString& nativeId) const;

    // ================= 検索パス =================

    QStringList searchPaths(PluginKind kind) const;
    void        setSearchPaths(PluginKind kind, const QStringList& paths);
    void        addSearchPath(PluginKind kind, const QString& path);
    static QStringList defaultSearchPaths(PluginKind kind);

    // ================= インスタンス生成 =================

    /// VST3 プラグインをロードしてホストを返す。所有権は呼び出し側。
    /// YAVE_ENABLE_VST3 が無効なビルドでは常に nullptr。
    std::unique_ptr<Vst3Host> createVst3(const QUuid& uid,
                                         double sampleRate, int maxBlockSize,
                                         QString* errorOut = nullptr);

    /// 字幕エフェクトのプロトタイプを取得する (一覧表示 / parameterSchema 用)。
    const yave::sdk::ISubtitleEffect* subtitleEffectPrototype(const QString& effectId) const;

    /// 字幕エフェクトの新しいインスタンスを生成する。
    /// prepare() の前計算結果をインスタンスが保持するため、
    /// クリップごとに個別のインスタンスが必要になる。
    SubtitleEffectPtr createSubtitleEffect(const QString& effectId) const;

    bool isSubtitleEffectMissing(const QString& effectId) const;

    // ================= AviUtl (Windows のみ有効) =================

    /// このビルド / プラットフォームで AviUtl が利用可能か。macOS では常に false。
    static bool isAviUtlSupported();

    // ================= ブラックリスト =================

    void blacklist(const QString& filePath, const QString& reason);
    void removeFromBlacklist(const QString& filePath);
    void clearBlacklist();
    bool isBlacklisted(const QString& filePath) const;
    std::vector<std::pair<QString, QString>> blacklistEntries() const;

    // ================= 終了処理 =================

    /// シャットダウン時に呼ぶ。オーディオ RT スレッドを止めた後でなければならない。
    void unloadAll();

signals:
    void scanStarted();
    void scanFinished();
    void pluginCrashed(const QString& filePath, const QString& reason);

private:
    PluginManager();
    ~PluginManager() override;

    void loadCache();
    void saveCache() const;

    Vst3Registry*           vst3_       = nullptr;
    SubtitleEffectRegistry* subtitleFx_ = nullptr;
    AviUtlRegistry*         aviutl_     = nullptr;   ///< 非 Windows では常に nullptr
    /// 再帰ロック。scanSync() はロックを保持したまま searchPaths() /
    /// subtitleEffectPrototype() / isBlacklisted() / blacklist() を呼ぶため、
    /// 非再帰 QMutex では起動時に自己デッドロックする。
    mutable QRecursiveMutex mutex_;

    struct BlacklistEntry { QString path; QString reason; };
    std::vector<BlacklistEntry> blacklist_;
};

} // namespace yave::plugin
