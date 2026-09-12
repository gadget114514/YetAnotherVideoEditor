#include "../src/core/Project.h"
#include "../src/core/Timeline.h"
#include "../src/core/Track.h"
#include "../src/core/AudioClip.h"
#include "../src/core/VideoClip.h"
#include "../src/core/commands/SetPropertyCommands.h"

#include <QtTest/QtTest>

using namespace yave;

class TestPropertyCommands : public QObject
{
    Q_OBJECT

private slots:
    void clipGainPanUndoRedo();
    void clipOpacityBlendUndoRedo();
    void clipRangeNameFade();
    void trackGainMutedUndoRedo();
    void trackOpacityBlendUndoRedo();
    void trackNameHeightVisible();
};

// ===========================================================================
//  クリップ (3.9 / 3.11 / 5.5)
// ===========================================================================

void TestPropertyCommands::clipGainPanUndoRedo()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Audio);
    auto clip = std::make_shared<AudioClip>(QUuid::createUuid());
    clip->setRange({0, 100});
    QVERIFY(t->insertClip(clip));
    const QUuid id = clip->id();

    project.undoStack()->push(
        new SetClipPropertyCommand(&project, id, ClipProperty::Gain, 0.6));
    project.undoStack()->push(
        new SetClipPropertyCommand(&project, id, ClipProperty::Pan, -0.3));
    QCOMPARE(clip->gain(), 0.6);
    QCOMPARE(clip->pan(), -0.3);

    project.undoStack()->undo();
    QCOMPARE(clip->pan(), 0.0);
    project.undoStack()->undo();
    QCOMPARE(clip->gain(), 1.0);

    project.undoStack()->redo();
    QCOMPARE(clip->gain(), 0.6);
}

void TestPropertyCommands::clipOpacityBlendUndoRedo()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Video);
    auto clip = std::make_shared<VideoClip>();
    clip->setRange({0, 100});
    QVERIFY(t->insertClip(clip));
    const QUuid id = clip->id();

    project.undoStack()->push(
        new SetClipPropertyCommand(&project, id, ClipProperty::Opacity, 0.4));
    project.undoStack()->push(
        new SetClipPropertyCommand(&project, id, ClipProperty::BlendMode,
                                   int(BlendMode::Add)));
    QCOMPARE(clip->opacity(), 0.4);
    QCOMPARE(clip->blendMode(), BlendMode::Add);

    project.undoStack()->undo();
    QCOMPARE(clip->blendMode(), BlendMode::Normal);
    project.undoStack()->undo();
    QCOMPARE(clip->opacity(), 1.0);
}

void TestPropertyCommands::clipRangeNameFade()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Video);
    auto clip = std::make_shared<VideoClip>();
    clip->setRange({0, 100});
    QVERIFY(t->insertClip(clip));
    const QUuid id = clip->id();

    project.undoStack()->push(
        new SetClipPropertyCommand(&project, id, ClipProperty::Name, QStringLiteral("hero")));
    project.undoStack()->push(
        new SetClipPropertyCommand(&project, id, ClipProperty::Start, qint64(200)));
    project.undoStack()->push(
        new SetClipPropertyCommand(&project, id, ClipProperty::Duration, qint64(50)));
    project.undoStack()->push(
        new SetClipPropertyCommand(&project, id, ClipProperty::FadeIn, qint64(10)));
    project.undoStack()->push(
        new SetClipPropertyCommand(&project, id, ClipProperty::FadeOut, qint64(5)));

    QCOMPARE(clip->name(), QStringLiteral("hero"));
    QCOMPARE(clip->range().start, int64_t(200));
    QCOMPARE(clip->range().duration, int64_t(50));
    QCOMPARE(clip->fadeInFrames(), int64_t(10));
    QCOMPARE(clip->fadeOutFrames(), int64_t(5));

    // Start 変更後もトラックのソート順が保たれている
    QVERIFY(std::is_sorted(t->clips().begin(), t->clips().end(),
                           [](const auto& a, const auto& b) {
                               return a->range().start < b->range().start;
                           }));

    project.undoStack()->undo();
    QCOMPARE(clip->fadeOutFrames(), int64_t(0));
    project.undoStack()->undo();
    QCOMPARE(clip->fadeInFrames(), int64_t(0));
    project.undoStack()->undo();
    QCOMPARE(clip->range().duration, int64_t(100));
    project.undoStack()->undo();
    QCOMPARE(clip->range().start, int64_t(0));
    project.undoStack()->undo();
    QCOMPARE(clip->name(), QString());
}

// ===========================================================================
//  トラック
// ===========================================================================

void TestPropertyCommands::trackGainMutedUndoRedo()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Audio);
    const QUuid id = t->id();

    project.undoStack()->push(
        new SetTrackPropertyCommand(&project, id, TrackProperty::Gain, 0.5));
    project.undoStack()->push(
        new SetTrackPropertyCommand(&project, id, TrackProperty::Muted, true));
    QCOMPARE(t->gain(), 0.5);
    QVERIFY(t->isMuted());

    project.undoStack()->undo();
    QVERIFY(!t->isMuted());
    project.undoStack()->undo();
    QCOMPARE(t->gain(), 1.0);
}

void TestPropertyCommands::trackOpacityBlendUndoRedo()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Video);
    const QUuid id = t->id();

    project.undoStack()->push(
        new SetTrackPropertyCommand(&project, id, TrackProperty::Opacity, 0.3));
    project.undoStack()->push(
        new SetTrackPropertyCommand(&project, id, TrackProperty::BlendMode,
                                   int(BlendMode::Screen)));
    QCOMPARE(t->opacity(), 0.3);
    QCOMPARE(t->blendMode(), BlendMode::Screen);

    project.undoStack()->undo();
    QCOMPARE(t->blendMode(), BlendMode::Normal);
    project.undoStack()->undo();
    QCOMPARE(t->opacity(), 1.0);
}

void TestPropertyCommands::trackNameHeightVisible()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Video);
    const QUuid id = t->id();
    const QString originalName = t->name();   ///< 既定名 (自動生成)

    project.undoStack()->push(
        new SetTrackPropertyCommand(&project, id, TrackProperty::Name, QStringLiteral("bg")));
    project.undoStack()->push(
        new SetTrackPropertyCommand(&project, id, TrackProperty::UiHeight, 120));
    project.undoStack()->push(
        new SetTrackPropertyCommand(&project, id, TrackProperty::Visible, false));

    QCOMPARE(t->name(), QStringLiteral("bg"));
    QCOMPARE(t->uiHeight(), 120);
    QVERIFY(!t->isVisible());

    project.undoStack()->undo();
    QVERIFY(t->isVisible());
    project.undoStack()->undo();
    QCOMPARE(t->uiHeight(), 64);
    project.undoStack()->undo();
    QCOMPARE(t->name(), originalName);
}

QTEST_MAIN(TestPropertyCommands)
#include "tst_propertycommands.moc"