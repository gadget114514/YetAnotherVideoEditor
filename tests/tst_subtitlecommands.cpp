#include "../src/core/Project.h"
#include "../src/core/Timeline.h"
#include "../src/core/Track.h"
#include "../src/core/commands/ImportSubtitleCommand.h"
#include "../src/subtitle/SubtitleClip.h"
#include "../src/subtitle/SubtitleStyle.h"
#include "../src/subtitle/commands/EditSubtitleTextCommand.h"
#include "../src/subtitle/commands/SetSubtitleStyleCommand.h"
#include "../src/subtitle/io/SrtParser.h"

#include <QtTest/QtTest>

using namespace yave;
using namespace yave::subtitle;

namespace {

std::vector<std::shared_ptr<Clip>> clipsFromSrt(Project& project, const QString& srt)
{
    const auto parsed = SrtParser::parseText(srt);
    QStringList warnings;
    auto clips = convertCuesToClips(parsed, project.timeline()->timebase(),
                                    QStringLiteral("default"), &warnings);
    std::vector<std::shared_ptr<Clip>> out;
    out.reserve(clips.size());
    for (auto& c : clips)
        out.push_back(std::move(c));
    return out;
}

} // anonymous namespace

class TestSubtitleCommands : public QObject
{
    Q_OBJECT

private slots:
    void importSrtBulk();
    void importSrtOverlapPolicy();
    void importUndoRestores();
    void editText();
    void editTextMerge();
    void setStyleField();
    void setStyleUndo();

private:
    template <typename T>
    T* findSubtitle(Project& p, const QString& text)
    {
        Timeline* tl = p.timeline();
        for (int t = 0; t < tl->trackCount(); ++t) {
            for (const auto& c : tl->trackAt(t)->clips()) {
                if (c->type() == ClipType::Subtitle) {
                    auto* sc = static_cast<SubtitleClip*>(c.get());
                    if (sc->plainText() == text)
                        return static_cast<T*>(sc);
                }
            }
        }
        return nullptr;
    }
};

void TestSubtitleCommands::importSrtBulk()
{
    Project project;
    const QString srt = QStringLiteral(
        "1\n00:00:01,000 --> 00:00:02,000\n一つ目\n\n"
        "2\n00:00:02,500 --> 00:00:03,500\n二つ目\n\n"
        "3\n00:00:04,000 --> 00:00:05,000\n三つ目\n\n");

    auto clips = clipsFromSrt(project, srt);
    QCOMPARE(int(clips.size()), 3);

    auto* cmd = new ImportSubtitleCommand(&project, std::move(clips),
                                          OverlapPolicy::SplitToNewTracks,
                                          -1, TrackType::Subtitle,
                                          QStringLiteral("test.srt"));
    project.undoStack()->push(cmd);

    // 新しい字幕トラックが 1 本作られ、3 クリップが載っている
    const std::vector<Track*> subs = project.timeline()->tracksOfType(TrackType::Subtitle);
    QCOMPARE(int(subs.size()), 1);
    QCOMPARE(int(subs[0]->clipCount()), 3);
    QCOMPARE(int(cmd->insertedCount()), 3);
    QCOMPARE(cmd->baseTrackIndex(), 0);

    // タイムスタンプが正しく変換されている
    auto* clip = findSubtitle<SubtitleClip>(project, QStringLiteral("一つ目"));
    QVERIFY(clip);
    QVERIFY(clip->range().start > 0);
}

void TestSubtitleCommands::importSrtOverlapPolicy()
{
    // 重なるキュー: 1〜3 秒 と 2〜4 秒
    Project project;
    const QString srt = QStringLiteral(
        "1\n00:00:01,000 --> 00:00:03,000\n先頭\n\n"
        "2\n00:00:02,000 --> 00:00:04,000\n重なり\n\n");

    // 既定: 別トラックへ振り分け → 2 トラック
    {
        Project p;
        auto clips = clipsFromSrt(p, srt);
        p.undoStack()->push(new ImportSubtitleCommand(
            &p, std::move(clips), OverlapPolicy::SplitToNewTracks, -1,
            TrackType::Subtitle, QStringLiteral("test.srt")));
        QCOMPARE(int(p.timeline()->tracksOfType(TrackType::Subtitle).size()), 2);
    }

    // SkipOverlapping: 重なるキューをスキップ → 1 トラック 1 クリップ
    {
        Project p;
        auto clips = clipsFromSrt(p, srt);
        auto* cmd = new ImportSubtitleCommand(
            &p, std::move(clips), OverlapPolicy::SkipOverlapping, -1,
            TrackType::Subtitle, QStringLiteral("test.srt"));
        p.undoStack()->push(cmd);
        const auto subs = p.timeline()->tracksOfType(TrackType::Subtitle);
        QCOMPARE(int(subs.size()), 1);
        QCOMPARE(int(subs[0]->clipCount()), 1);
        QCOMPARE(int(cmd->insertedCount()), 1);
    }
}

void TestSubtitleCommands::importUndoRestores()
{
    Project project;
    const QString srt = QStringLiteral("1\n00:00:01,000 --> 00:00:02,000\nあ\n\n");
    auto clips = clipsFromSrt(project, srt);
    project.undoStack()->push(new ImportSubtitleCommand(
        &project, std::move(clips), OverlapPolicy::SplitToNewTracks, -1,
        TrackType::Subtitle, QStringLiteral("test.srt")));

    QCOMPARE(int(project.timeline()->trackCount()), 1);

    project.undoStack()->undo();
    QCOMPARE(int(project.timeline()->trackCount()), 0);

    project.undoStack()->redo();
    QCOMPARE(int(project.timeline()->trackCount()), 1);
    QCOMPARE(int(project.timeline()->trackAt(0)->clipCount()), 1);
}

void TestSubtitleCommands::editText()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Subtitle);
    auto clip = std::make_shared<SubtitleClip>();
    clip->setRange({0, 100});
    clip->setPlainText(QStringLiteral("元の文章"));
    t->insertClip(clip);

    const QUuid id = clip->id();
    project.undoStack()->push(new EditSubtitleTextCommand(&project, id,
                                                          QStringLiteral("編集後")));

    SubtitleClip* sc = findSubtitle<SubtitleClip>(project, QStringLiteral("編集後"));
    QVERIFY(sc);

    project.undoStack()->undo();
    QVERIFY(findSubtitle<SubtitleClip>(project, QStringLiteral("元の文章")));

    project.undoStack()->redo();
    QVERIFY(findSubtitle<SubtitleClip>(project, QStringLiteral("編集後")));
}

void TestSubtitleCommands::editTextMerge()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Subtitle);
    auto clip = std::make_shared<SubtitleClip>();
    clip->setRange({0, 100});
    clip->setPlainText(QString());
    t->insertClip(clip);

    const QUuid id = clip->id();
    project.undoStack()->push(new EditSubtitleTextCommand(&project, id, QStringLiteral("a")));
    project.undoStack()->push(new EditSubtitleTextCommand(&project, id, QStringLiteral("ab")));
    project.undoStack()->push(new EditSubtitleTextCommand(&project, id, QStringLiteral("abc")));

    // 連続入力は 1 コマンドにマージされ、1 回の Undo で消える
    project.undoStack()->undo();
    QVERIFY(findSubtitle<SubtitleClip>(project, QStringLiteral("")));
}

void TestSubtitleCommands::setStyleField()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Subtitle);
    auto clip = std::make_shared<SubtitleClip>();
    clip->setRange({0, 100});
    clip->setPlainText(QStringLiteral("スタイル"));
    t->insertClip(clip);

    const QUuid id = clip->id();
    project.undoStack()->push(new SetSubtitleStyleCommand(
        &project, id, SubtitleStyleField::FontPointSize, QVariant(96.0)));
    project.undoStack()->push(new SetSubtitleStyleCommand(
        &project, id, SubtitleStyleField::FillColor, QVariant(QColor("#ff0000"))));

    SubtitleClip* sc = findSubtitle<SubtitleClip>(project, QStringLiteral("スタイル"));
    QVERIFY(sc);
    const SubtitleStyleDiff& d = sc->styleOverride();
    QVERIFY(d.fontPointSize.has_value());
    QCOMPARE(*d.fontPointSize, 96.0);
    QVERIFY(d.fillColor.has_value());
    QCOMPARE(d.fillColor->name(), QStringLiteral("#ff0000"));

    // Undo で戻る
    project.undoStack()->undo();
    sc = findSubtitle<SubtitleClip>(project, QStringLiteral("スタイル"));
    QVERIFY(!sc->styleOverride().fillColor.has_value());
}

void TestSubtitleCommands::setStyleUndo()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Subtitle);
    auto clip = std::make_shared<SubtitleClip>();
    clip->setRange({0, 100});
    clip->setPlainText(QStringLiteral("undo"));
    t->insertClip(clip);

    const QUuid id = clip->id();
    project.undoStack()->push(new SetSubtitleStyleCommand(
        &project, id, SubtitleStyleField::FontFamily, QVariant(QStringLiteral("Impact"))));

    SubtitleClip* sc = findSubtitle<SubtitleClip>(project, QStringLiteral("undo"));
    QCOMPARE(*sc->styleOverride().fontFamily, QStringLiteral("Impact"));

    project.undoStack()->undo();
    sc = findSubtitle<SubtitleClip>(project, QStringLiteral("undo"));
    QVERIFY(!sc->styleOverride().fontFamily.has_value());
}

QTEST_MAIN(TestSubtitleCommands)
#include "tst_subtitlecommands.moc"