#include "LanguageManager.h"

#include "../util/Log.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QLibraryInfo>
#include <QLocale>
#include <QQmlEngine>
#include <QSettings>
#include <QWidget>

namespace yave {

LanguageManager& LanguageManager::instance()
{
    static LanguageManager s;
    return s;
}

QString LanguageManager::displayName(const QString& code) const
{
    if (code == QLatin1String("ja"))
        return QString::fromUtf8("日本語");
    if (code == QLatin1String("en"))
        return QStringLiteral("English");
    return code;
}

void LanguageManager::initialize()
{
    // QSettings からロケール決定。未設定ならシステムロケールから推定する。
    QSettings settings;
    QString lang = settings.value(QStringLiteral("ui/language")).toString();
    if (lang.isEmpty()) {
        const QString system = QLocale::system().name();   ///< "ja_JP" 等
        lang = system.startsWith(QLatin1String("ja")) ? QStringLiteral("ja")
                                                      : QStringLiteral("en");
    }
    if (!availableLanguages().contains(lang))
        lang = QStringLiteral("en");

    current_ = lang;
    loadTranslatorsFor(lang);
}

void LanguageManager::setLanguage(const QString& code)
{
    if (code == current_)
        return;
    if (!availableLanguages().contains(code))
        return;

    const QString previous = current_;

    // ---- (1) 既存のトランスレータをすべて外す ----
    removeAllTranslators();

    // ---- (2) 新しい言語をロード ----
    if (!loadTranslatorsFor(code)) {
        qCWarning(lcI18n) << "Failed to switch to" << code << "; reverting to" << previous;
        loadTranslatorsFor(previous);
        return;
    }
    current_ = code;

    // ---- (3) 設定を保存 ----
    QSettings settings;
    settings.setValue(QStringLiteral("ui/language"), code);

    // ---- (4) UI フォントを言語に合わせる ----
    applyUiFont(code);

    // ---- (5) QML の qsTr() バインディングを再評価させる ----
    // これ 1 回で、qsTr() を使っているすべてのバインディングが再計算される。
    if (qmlEngine_)
        qmlEngine_->retranslate();

    // ---- (6) Widgets 側へ LanguageChange イベントを送る ----
    for (QWidget* w : QApplication::topLevelWidgets())
        QCoreApplication::sendEvent(w, new QEvent(QEvent::LanguageChange));

    // ---- (7) 自前で文字列を保持しているものへ通知する ----
    emit languageChanged();

    qCInfo(lcI18n) << "UI language switched:" << previous << "->" << code;
}

bool LanguageManager::loadTranslatorsFor(const QString& code)
{
    // ---- アプリ本体 ----
    // 英語はソース文字列がそのまま使われるため、.qm が無くても正常。
    auto appTr = std::make_unique<QTranslator>();
    const QString appQm = QStringLiteral(":/i18n/yave_%1.qm").arg(code);
    if (appTr->load(appQm)) {
        QCoreApplication::installTranslator(appTr.get());
        appTranslator_ = std::move(appTr);
    } else if (code != QLatin1String("en")) {
        qCWarning(lcI18n) << "Translation not found:" << appQm;
        return false;
    }

    // ---- Qt 標準ダイアログ / ウィジェット ----
    // これを忘れると、ファイルダイアログの「開く」「キャンセル」だけが
    // 前の言語のまま残る。
    auto qtTr = std::make_unique<QTranslator>();
    const QString qtDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (qtTr->load(QStringLiteral("qtbase_%1").arg(code), qtDir)) {
        QCoreApplication::installTranslator(qtTr.get());
        qtBaseTranslator_ = std::move(qtTr);
    }

    // ---- プラグイン同梱翻訳 ----
    for (const PluginTrSource& src : pluginSources_) {
        auto tr = std::make_unique<QTranslator>();
        if (tr->load(QStringLiteral("%1_%2").arg(src.prefix, code), src.dir)) {
            QCoreApplication::installTranslator(tr.get());
            pluginTranslators_.push_back(std::move(tr));
        }
    }
    return true;
}

void LanguageManager::removeAllTranslators()
{
    for (auto& t : pluginTranslators_) {
        QCoreApplication::removeTranslator(t.get());
    }
    pluginTranslators_.clear();

    if (qtBaseTranslator_) {
        QCoreApplication::removeTranslator(qtBaseTranslator_.get());
        qtBaseTranslator_.reset();
    }
    if (appTranslator_) {
        QCoreApplication::removeTranslator(appTranslator_.get());
        appTranslator_.reset();
    }
}

void LanguageManager::registerPluginTranslations(const QString& dir, const QString& prefix)
{
    // 既に登録済みなら何もしない
    for (const auto& s : pluginSources_)
        if (s.dir == dir && s.prefix == prefix)
            return;

    pluginSources_.push_back({dir, prefix});

    // 現在の言語で即座にロードする
    auto tr = std::make_unique<QTranslator>();
    if (tr->load(QStringLiteral("%1_%2").arg(prefix, current_), dir)) {
        QCoreApplication::installTranslator(tr.get());
        pluginTranslators_.push_back(std::move(tr));
        emit languageChanged();      ///< エフェクト一覧の表示名を更新させる
    }
}

QString LanguageManager::translateKey(const QString& key, const QString& context)
{
    // 翻訳が見つからなければ QCoreApplication::translate() はキー自身を返す。
    // フォールバックのための特別な分岐が要らない。
    return QCoreApplication::translate(context.toUtf8().constData(),
                                       key.toUtf8().constData());
}

void LanguageManager::applyUiFont(const QString& code)
{
    // 日本語 UI でシステムフォントに日本語グリフが無いと豆腐になる。
    // 字幕レンダリング用フォントは SubtitleStyle が持つため影響を受けない。
    QFont f = QApplication::font();
    if (code == QLatin1String("ja"))
        f.setFamilies({QStringLiteral("Yu Gothic UI"), QStringLiteral("Meiryo"),
                       QStringLiteral("Noto Sans JP")});
    QApplication::setFont(f);
}

} // namespace yave
