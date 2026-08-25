#include "../src/i18n/LanguageManager.h"
#include "../src/util/TimecodeFormat.h"
#include "../src/core/Rational.h"

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QLocale>

using namespace yave;

class TestI18n : public QObject
{
    Q_OBJECT

private slots:
    void switchLanguageAtRuntime();
    void timecodeIsLocaleIndependent();
};

void TestI18n::switchLanguageAtRuntime()
{
    auto& lm = LanguageManager::instance();
    lm.initialize();

    lm.setLanguage(QStringLiteral("en"));
    QCOMPARE(QCoreApplication::translate("QObject", "Export"), QStringLiteral("Export"));

    QSignalSpy spy(&lm, &LanguageManager::languageChanged);
    lm.setLanguage(QStringLiteral("ja"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(lm.currentLanguage(), QStringLiteral("ja"));
    
    // yave_ja.ts に "Export" -> "書き出し" が入っている前提
    QCOMPARE(QCoreApplication::translate("QObject", "Export"), QString::fromUtf8("書き出し"));

    // 戻せることも確認
    lm.setLanguage(QStringLiteral("en"));
    QCOMPARE(QCoreApplication::translate("QObject", "Export"), QStringLiteral("Export"));
}

void TestI18n::timecodeIsLocaleIndependent()
{
    // 小数点が ',' になるロケール (ドイツ語)
    QLocale::setDefault(QLocale(QLocale::German));
    
    // formatTimecode
    QCOMPARE(formatTimecode(3600, timebase::Fps60, false), QStringLiteral("00:01:00:00"));
    QCOMPARE(formatTimecode(1800, timebase::Fps30, true), QStringLiteral("00:01:00;02"));

    // parseTimecode
    auto parsed1 = parseTimecode(QStringLiteral("00:01:00:00"), timebase::Fps60);
    QVERIFY(parsed1.has_value());
    QCOMPARE(parsed1.value(), 3600);

    auto parsed2 = parseTimecode(QStringLiteral("00:01:00;02"), timebase::Fps30);
    QVERIFY(parsed2.has_value());
    QCOMPARE(parsed2.value(), 1800);
}

QTEST_MAIN(TestI18n)
#include "tst_i18n.moc"
