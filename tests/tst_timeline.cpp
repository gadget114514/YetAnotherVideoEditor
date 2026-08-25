#include "../src/core/Project.h"
#include "../src/core/Timeline.h"
#include "../src/core/Track.h"
#include "../src/core/VideoClip.h"
#include "../src/core/AudioClip.h"
#include "../src/core/AiPlaceholderClip.h"
#include "../src/core/commands/AddClipCommand.h"
#include "../src/core/commands/AddTrackCommand.h"
#include "../src/core/commands/SplitClipCommand.h"

#include <QtTest/QtTest>

using namespace yave;


namespace {
TimeRange TR(int64_t start, int64_t duration) { return TimeRange{start, duration}; }
}

class TestTimeline : public QObject
{
    Q_OBJECT


private slots:
    void trackInsertRemove();
    void clipOverlapRejection();
    void clipAtBinarySearch();
    void clipsIn();
    void moveTrackZOrder();
    void snapshotBuilding();
    void splitCommandUndoRedo();
    void rippleDelete();
    void undoStackIntegration();

private:
    std::shared_ptr<VideoClip> makeClip(int64_t start, int64_t duration);
};

std::shared_ptr<VideoClip> TestTimeline::makeClip(int64_t start, int64_t duration)
{
    auto c = std::make_shared<VideoClip>();
    c->setRange({start, duration});
    return c;
}

void TestTimeline::trackInsertRemove()
{
    Project project;
    Timeline* tl = project.timeline();

    Track* v = tl->appendTrack(TrackType::Video);
    Track* a = tl->appendTrack(TrackType::Audio);
    QCOMPARE(tl->trackCount(), 2);
    QCOMPARE(tl->indexOfTrack(v), 0);
    QCOMPARE(tl->indexOfTrack(a), 1);

    // Z オーダー: index 0 が最背面
    auto removed = tl->takeTrackById(a->id());
    QVERIFY(removed != nullptr);
    QCOMPARE(tl->trackCount(), 1);

    // 元の位置へ戻す
    tl->reinsertTrack(1, std::move(removed));
    QCOMPARE(tl->trackCount(), 2);
    QVERIFY(tl->trackById(a->id()) != nullptr);
}

void TestTimeline::clipOverlapRejection()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Video);

    QVERIFY(t->insertClip(makeClip(0, 100)));
    QVERIFY(!t->insertClip(makeClip(50, 100)));     ///< 完全重複 -> 拒否
    QVERIFY(!t->insertClip(makeClip(-50, 60)));     ///< 前方掛け -> 拒否
    QVERIFY(!t->insertClip(makeClip(90, 20)));      ///< 後方掛け -> 拒否
    QVERIFY(t->insertClip(makeClip(100, 50)));      ///< 隣接 -> OK (半開区間)
    QCOMPARE(int(t->clipCount()), 2);

    // 空クリップは拒否
    QVERIFY(!t->insertClip(makeClip(200, 0)));
}

void TestTimeline::clipAtBinarySearch()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Video);

    for (int i = 0; i < 10; ++i)
        QVERIFY(t->insertClip(makeClip(i * 100, 100)));

    for (int f = 0; f < 1000; ++f) {
        const auto c = t->clipAt(f);
        QVERIFY(c != nullptr);
        QCOMPARE(c->range().start, int64_t((f / 100) * 100));
    }
    QVERIFY(t->clipAt(1000) == nullptr);   ///< 半開区間の終端

    QCOMPARE(t->contentDuration(), 1000);
    QCOMPARE(t->nextClipStart(-1), int64_t(0));
    QCOMPARE(t->prevClipEnd(500), int64_t(500));
}

void TestTimeline::clipsIn()
{
    Project project;
    Track* t = project.timeline()->appendTrack(TrackType::Video);

    QVERIFY(t->insertClip(makeClip(0, 50)));
    QVERIFY(t->insertClip(makeClip(50, 50)));
    QVERIFY(t->insertClip(makeClip(200, 100)));

    const auto inRange = t->clipsIn({40, 30});
    QCOMPARE(int(inRange.size()), 2);   ///< [0,50) と [50,100)

    const auto none = t->clipsIn({150, 40});
    QCOMPARE(int(none.size()), 0);
}

void TestTimeline::moveTrackZOrder()
{
    Project project;
    Timeline* tl = project.timeline();

    tl->appendTrack(TrackType::Video);   ///< A (index 0)
    tl->appendTrack(TrackType::Video);   ///< B (index 1)
    tl->appendTrack(TrackType::Video);   ///< C (index 2)

    Track* a = tl->trackAt(0);
    Track* c = tl->trackAt(2);

    tl->moveTrack(0, 2);                 ///< A を最前面へ
    QCOMPARE(tl->trackAt(2), a);
    QCOMPARE(tl->indexOfTrack(c), 1);

    // Undo 相当: A を元へ戻す (rotate の性質上、単純な引数交換では戻らない)
    auto moved = tl->takeTrack(2);
    tl->reinsertTrack(0, std::move(moved));
    QCOMPARE(tl->trackAt(0), a);
    QCOMPARE(tl->trackAt(2), c);
}

void TestTimeline::snapshotBuilding()
{
    Project project;
    Timeline* tl = project.timeline();

    Track* bg = tl->appendTrack(TrackType::Video);
    auto clip = makeClip(0, 1000);
    clip->setOpacity(0.5);
    clip->setBlendMode(BlendMode::Add);
    QVERIFY(bg->insertClip(clip));

    Track* audio = tl->appendTrack(TrackType::Audio);   ///< 音声はスナップショット外
    Q_UNUSED(audio);

    const RenderSnapshot snap = tl->buildSnapshot(500);
    QCOMPARE(snap.frameIndex, int64_t(500));
    QCOMPARE(snap.timebase.num, tl->timebase().num);
    QCOMPARE(snap.layers.size(), size_t(1));
    QCOMPARE(snap.layers[0].zIndex, 0);          ///< 最初の可視トラックが z=0
    QCOMPARE(snap.layers[0].blendMode, BlendMode::Add);
    QCOMPARE(snap.layers[0].opacity, 0.5f);

    // 範囲外フレームにはレイヤーがない
    const RenderSnapshot emptySnap = tl->buildSnapshot(99999);
    QCOMPARE(emptySnap.layers.size(), size_t(0));
}

void TestTimeline::splitCommandUndoRedo()
{
    Project project;
    Timeline* tl = project.timeline();
    Track* t = tl->appendTrack(TrackType::Video);

    auto original = makeClip(0, 1000);
    original->setSourceOffset(500);
    original->setName(QStringLiteral("orig"));
    const QUuid originalId = original->id();

    auto addCmd = new AddClipCommand(&project, t->id(), 0, original);
    project.undoStack()->push(addCmd);
    QCOMPARE(int(t->clipCount()), 1);

    project.undoStack()->push(new SplitClipCommand(&project, t->id(),
                                                   originalId, 400));
    QCOMPARE(int(t->clipCount()), 2);
    QVERIFY(t->clipById(originalId) == nullptr);       ///< 分割後は id 不在

    // 左右の範囲とソースオフセットを検証
    const auto& clips = t->clips();
    QCOMPARE(clips[0]->range(), TR(0,400));
    QCOMPARE(clips[1]->range(), TR(400,600));
    QCOMPARE(clips[1]->sourceOffset(), int64_t(900));   ///< 500 + 400

    // ---- Undo ----
    project.undoStack()->undo();
    QCOMPARE(int(t->clipCount()), 1);
    QVERIFY(t->clipById(originalId) != nullptr);
    QCOMPARE(t->clipById(originalId)->range(), TR(0,1000));

    // ---- Redo ----
    project.undoStack()->redo();
    QCOMPARE(int(t->clipCount()), 2);
}

void TestTimeline::rippleDelete()
{
    Project project;
    Timeline* tl = project.timeline();
    Track* t = tl->appendTrack(TrackType::Video);

    QVERIFY(t->insertClip(makeClip(0, 100)));
    QVERIFY(t->insertClip(makeClip(100, 100)));
    QVERIFY(t->insertClip(makeClip(200, 100)));

    std::vector<int> tracks{0};
    project.undoStack()->push(new RippleDeleteCommand(&project, tracks, TR(100, 100)));

    // 中央が削除され、後続が詰められる
    QCOMPARE(int(t->clipCount()), 2);
    QCOMPARE(t->clips()[1]->range(), TR(100,100));

    // Undo で元通り
    project.undoStack()->undo();
    QCOMPARE(int(t->clipCount()), 3);
    QCOMPARE(t->clips()[1]->range(), TR(100,100));
    QCOMPARE(t->clips()[2]->range(), TR(200,100));
}

void TestTimeline::undoStackIntegration()
{
    Project project;
    Timeline* tl = project.timeline();
    QCOMPARE(tl->trackCount(), 0);   ///< 生の Project は既定トラックを持たない

    // トラック追加も Undo 可能
    project.undoStack()->push(new AddTrackCommand(&project, TrackType::Video, 0));
    QCOMPARE(tl->trackCount(), 1);

    project.undoStack()->undo();
    QCOMPARE(tl->trackCount(), 0);

    project.undoStack()->redo();
    QCOMPARE(tl->trackCount(), 1);
}

QTEST_MAIN(TestTimeline)
#include "tst_timeline.moc"
