#pragma once

#include <QObject>
#include <QStringList>
#include <QTranslator>

#include <memory>
#include <vector>

class QQmlEngine;

namespace yave {

/// 日本語 / 英語のランタイム切替を管理する (10章)。
///
/// 再起動不要の切り替え手順:
///   (1) 既存トランスレータをすべて外す
///   (2) 新しい .qm をロードして install
///   (3) QSettings へ保存
///   (4) UI フォント調整
///   (5) QQmlEngine::retranslate() で qsTr() バインディングを再評価
///   (6) Widgets 側へ QEvent::LanguageChange を送信
///   (7) languageChanged シグナルで動的文字列を更新
class LanguageManager : public QObject
{
    Q_OBJECT
public:
    static LanguageManager& instance();

    /// main() の最初期 (QML エンジン生成前) に呼ぶ。
    void initialize();

    QStringList availableLanguages() const { return {QStringLiteral("ja"),
                                                    QStringLiteral("en")}; }
    QString currentLanguage() const { return current_; }
    QString displayName(const QString& code) const;

    void setLanguage(const QString& code);

    /// retranslate() を呼ぶためにエンジンを登録する。
    void setQmlEngine(QQmlEngine* engine) { qmlEngine_ = engine; }

    /// プラグイン同梱翻訳の登録 (SubtitleEffectLoader から呼ばれる)。
    void registerPluginTranslations(const QString& dir, const QString& prefix);

    /// 翻訳キーの解決。翻訳が無ければキー自身が返る (フォールバック不要設計)。
    static QString translateKey(const QString& key, const QString& context = {});

signals:
    void languageChanged();

private:
    LanguageManager() = default;

    bool   loadTranslatorsFor(const QString& code);
    void   removeAllTranslators();
    void   applyUiFont(const QString& code);

    struct PluginTrSource
    {
        QString dir;
        QString prefix;
    };

    QString current_;
    QQmlEngine* qmlEngine_ = nullptr;

    std::unique_ptr<QTranslator> appTranslator_;
    std::unique_ptr<QTranslator> qtBaseTranslator_;
    std::vector<PluginTrSource>  pluginSources_;
    std::vector<std::unique_ptr<QTranslator>> pluginTranslators_;
};

} // namespace yave
