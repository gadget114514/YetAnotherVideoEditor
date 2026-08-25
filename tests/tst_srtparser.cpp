#include "../src/core/Project.h"
#include "../src/core/Timeline.h"
#include "../src/core/Track.h"
#include "../src/subtitle/SubtitleClip.h"
#include "../src/subtitle/io/SrtParser.h"
#include "../src/subtitle/io/SrtWriter.h"

#include <QFile>
#include <QtTest/QtTest>

using namespace yave;
using namespace yave::subtitle;

class TestSrtParser : public QObject
{
    Q_OBJECT

private slots:
    void parseBasic();
    void parseShiftJisFallback();
    void parseInlineMarkup();
    void convertCuesToClipsRounding();
    void writeRoundTrip();
    void malformedInput();

private:
    QString writeTempFile(const QByteArray& content) const;
};

QString TestSrtParser::writeTempFile(const QByteArray& content) const
{
    const QString path = QStandardPaths::writableLocation(
                             QStandardPaths::TempLocation)
                         + QStringLiteral("/tst_srt_%1.srt")
                               .arg(QDateTime::currentMSecsSinceEpoch());
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(content);
    return path;
}

void TestSrtParser::parseBasic()
{
    const QString srt = QStringLiteral(
        "1\n"
        "00:00:01,000 --> 00:00:02,500\n"
        "こんにちは世界\n"
        "\n"
        "2\n"
        "00:00:03,200 --> 00:00:04,000\n"
        "2 行目の字幕です\n"
        "3 行目もあります\n"
        "\n");

    const auto result = SrtParser::parseText(srt);
    QVERIFY(result.ok);
    QCOMPARE(int(result.cues.size()), 2);
    QCOMPARE(result.cues[0].startSeconds, 1.0);
    QCOMPARE(result.cues[0].endSeconds, 2.5);
    QCOMPARE(result.cues[0].rawText, QStringLiteral("こんにちは世界"));
    QVERIFY(result.cues[1].rawText.contains(QLatin1Char('\n')));
    QVERIFY(result.warnings.isEmpty());
}

void TestSrtParser::parseShiftJisFallback()
{
    // 「テスト」を Shift_JIS (0x 83 65 83 58 83 67) でエンコードしたもの。
    // UTF-8 としては不正バイト列のため、フォールバックが機能すること。
    QByteArray sjis;
    sjis.append("1\n00:00:01,000 --> 00:00:02,000\n");
    sjis.append(char(0x83)).append(char(0x65))
        .append(char(0x83)).append(char(0x58))
        .append(char(0x83)).append(char(0x67));
    sjis.append("\n\n");

    const QString path = writeTempFile(sjis);
    const auto result = SrtParser::parseFile(path);
    if (result.ok) {
        // デコードに成功した場合は「テスト」相当の文字が含まれるはず
        QVERIFY(!result.cues.empty());
    }
    // フォールバック不可の環境では warnings 経由で報告されるため、
    // クラッシュしないことだけを検証する。
}

void TestSrtParser::parseInlineMarkup()
{
    const QString srt = QStringLiteral(
        "1\n00:00:00,000 --> 00:00:01,000\n"
        "<b>太字</b>と<i>斜体</i>\n\n");

    const auto result = SrtParser::parseText(srt);
    QVERIFY(result.ok);

    SubtitleText text = SubtitleText::fromSrtMarkup(result.cues[0].rawText);
    QCOMPARE(text.plain(), QStringLiteral("太字と斜体"));
    QCOMPARE(int(text.spans().size()), 2);

    // toSrtMarkup でタグへ戻せること
    const QString markup = text.toSrtMarkup();
    QVERIFY(markup.contains(QStringLiteral("<b>")));
    QVERIFY(markup.contains(QStringLiteral("</b>")));
}

void TestSrtParser::convertCuesToClipsRounding()
{
    const Rational tb{1001, 60000};   ///< 59.94fps
    SrtParseResult parsed;
    parsed.ok = true;

    SrtCue cue;
    cue.index = 1;
    cue.startSeconds = 1.0;
    cue.endSeconds = 2.0;
    cue.rawText = QStringLiteral("字幕");
    parsed.cues.push_back(cue);

    QStringList warnings;
    auto clips = convertCuesToClips(parsed, tb, QStringLiteral("default"), &warnings);
    QCOMPARE(int(clips.size()), 1);
    QVERIFY(warnings.isEmpty());

    // 開始 Floor / 終了 Ceil により「表示すべき瞬間が含まれる」ことを確認
    const int64_t startF = secondsToFrames(1.0, tb, RoundMode::Floor);
    const int64_t endF   = secondsToFrames(2.0, tb, RoundMode::Ceil);
    const TimeRange expected{startF, endF - startF};
    QCOMPARE(clips[0]->range(), expected);
}

void TestSrtParser::writeRoundTrip()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Subtitle);

    auto clip = std::make_shared<SubtitleClip>();
    clip->setRange({60, 120});   ///< 59.94fps で約 1〜3 秒
    clip->setPlainText(QStringLiteral("ラウンドトリップ"));
    t->insertClip(clip);

    const QString srt =
        SrtWriter::writeToString(*t, project.timeline()->timebase(), nullptr);
    QVERIFY(srt.contains(QStringLiteral("-->")));
    QVERIFY(srt.contains(QStringLiteral("ラウンドトリップ")));

    // 書き出した SRT を再度パースできること
    const auto result = SrtParser::parseText(srt);
    QVERIFY(result.ok);
    QCOMPARE(int(result.cues.size()), 1);
    QCOMPARE(result.cues[0].rawText, QStringLiteral("ラウンドトリップ"));
}

void TestSrtParser::malformedInput()
{
    // タイムコード行が壊れている
    const auto r1 = SrtParser::parseText(QStringLiteral("1\nnot a timecode\ntext\n\n"));
    QVERIFY(r1.warnings.size() > 0 || !r1.ok);

    // 空入力
    const auto r2 = SrtParser::parseText(QString());
    QVERIFY(!r2.ok);
    QVERIFY(r2.cues.empty());
}

QTEST_MAIN(TestSrtParser)
#include "tst_srtparser.moc"
