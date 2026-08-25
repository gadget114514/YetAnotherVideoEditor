#include "../src/core/Project.h"
#include "../src/core/Timeline.h"
#include "../src/core/Track.h"
#include "../src/core/Transition.h"
#include "../src/core/VideoClip.h"
#include "../src/core/VideoFilter.h"
#include "../src/core/commands/FilterCommands.h"
#include "../src/core/commands/TransitionCommands.h"
#include "../src/io/ProjectSerializer.h"
#include "../src/subtitle/TitleClip.h"

#include <QtTest/QtTest>

using namespace yave;

namespace {

/// ハンドル付きのクリップ (ソースの前後に余裕がある) を作る。
std::shared_ptr<VideoClip> makeClip(int64_t start, int64_t duration,
                                    int64_t sourceOffset, int64_t sourceLength)
{
    auto c = std::make_shared<VideoClip>(QUuid::createUuid());
    c->setRange({start, duration});
    c->setSourceOffset(sourceOffset);
    c->setMaxDurationFrames(sourceLength);
    return c;
}

} // anonymous namespace

class TestTransitionFilter : public QObject
{
    Q_OBJECT

private slots:
    void filterStackOrder();
    void filterParamsResolve();
    void transitionNeedsBoundary();
    void transitionClampedToHandles();
    void transitionRejectedWithoutHandles();
    void transitionDroppedWhenClipRemoved();
    void transitionProgress();
    void snapshotEmitsPairDuringTransition();
    void commandsAreUndoable();
    void serializerRoundTrip();
};

// ===========================================================================
//  フィルタ (3.9)
// ===========================================================================

void TestTransitionFilter::filterStackOrder()
{
    auto clip = makeClip(0, 100, 0, 100);

    clip->addFilter({ QString::fromLatin1(builtinFilter::kBlur), {}, true });
    clip->addFilter({ QString::fromLatin1(builtinFilter::kMono), {}, true });
    QCOMPARE(int(clip->filters().size()), 2);

    clip->moveFilter(1, 0);
    QCOMPARE(clip->filters()[0].filterId, QString::fromLatin1(builtinFilter::kMono));

    // 無効な段は解決結果から落ちる
    clip->setFilterEnabled(0, false);
    QCOMPARE(int(clip->resolvedFilters().size()), 1);
    QCOMPARE(clip->resolvedFilters()[0].filterId, QString::fromLatin1(builtinFilter::kBlur));

    // clone はフィルタも複製する
    auto copy = clip->clone();
    QCOMPARE(int(copy->filters().size()), 2);
    QVERIFY(copy->id() != clip->id());
}

void TestTransitionFilter::filterParamsResolve()
{
    VideoFilterInstance inst;
    inst.filterId = QString::fromLatin1(builtinFilter::kColorAdjust);
    inst.params   = { { QStringLiteral("brightness"), 0.25 },
                      { QStringLiteral("contrast"),   1.5 },
                      { QStringLiteral("saturation"), 0.5 },
                      { QStringLiteral("gamma"),      2.0 } };

    const auto p = resolveFilterParams(inst);
    QCOMPARE(p[0], 0.25f);
    QCOMPARE(p[1], 1.5f);
    QCOMPARE(p[2], 0.5f);
    QCOMPARE(p[3], 2.0f);

    // 未知のフィルタは全 0
    VideoFilterInstance unknown;
    unknown.filterId = QStringLiteral("nope");
    QCOMPARE(resolveFilterParams(unknown)[0], 0.0f);
}

// ===========================================================================
//  トランジション (3.10)
// ===========================================================================

void TestTransitionFilter::transitionNeedsBoundary()
{
    Track track(TrackType::Video);
    QVERIFY(track.insertClip(makeClip(0, 100, 30, 300)));
    QVERIFY(track.insertClip(makeClip(100, 100, 30, 300)));

    Transition t;
    t.transitionId   = QString::fromLatin1(builtinTransition::kDissolve);
    t.centerFrame    = 50;          ///< 境界ではない
    t.durationFrames = 20;

    QString error;
    QVERIFY(!track.addTransition(t, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(track.transitions().empty());

    t.centerFrame = 100;            ///< ちょうど境界
    QVERIFY(track.addTransition(t, &error));
    QCOMPARE(int(track.transitions().size()), 1);
    QCOMPARE(track.transitions()[0].fromClipId, track.clips()[0]->id());
    QCOMPARE(track.transitions()[0].toClipId,   track.clips()[1]->id());
}

void TestTransitionFilter::transitionClampedToHandles()
{
    Track track(TrackType::Video);
    // 前のクリップは out 点の後ろに 10 フレームしか残っていない
    QVERIFY(track.insertClip(makeClip(0, 100, 0, 110)));
    QVERIFY(track.insertClip(makeClip(100, 100, 50, 300)));

    QCOMPARE(track.maxTransitionDuration(100), int64_t(20));   ///< min(10, 50) * 2

    Transition t;
    t.transitionId   = QString::fromLatin1(builtinTransition::kDissolve);
    t.centerFrame    = 100;
    t.durationFrames = 120;          ///< ハンドルを大きく超える

    QVERIFY(track.addTransition(t));
    QCOMPARE(track.transitions()[0].durationFrames, int64_t(20));   ///< 縮められる
}

void TestTransitionFilter::transitionRejectedWithoutHandles()
{
    Track track(TrackType::Video);
    // 両側ともソースを使い切っている = ハンドルが無い
    QVERIFY(track.insertClip(makeClip(0, 100, 0, 100)));
    QVERIFY(track.insertClip(makeClip(100, 100, 0, 100)));

    QCOMPARE(track.maxTransitionDuration(100), int64_t(0));

    Transition t;
    t.transitionId   = QString::fromLatin1(builtinTransition::kDissolve);
    t.centerFrame    = 100;
    t.durationFrames = 30;

    QString error;
    QVERIFY(!track.addTransition(t, &error));
    QVERIFY(!error.isEmpty());
}

void TestTransitionFilter::transitionDroppedWhenClipRemoved()
{
    Track track(TrackType::Video);
    auto a = makeClip(0, 100, 30, 300);
    auto b = makeClip(100, 100, 30, 300);
    QVERIFY(track.insertClip(a));
    QVERIFY(track.insertClip(b));

    Transition t;
    t.transitionId   = QString::fromLatin1(builtinTransition::kDissolve);
    t.centerFrame    = 100;
    t.durationFrames = 20;
    QVERIFY(track.addTransition(t));

    // 片側を消すと境界そのものが無くなるので、トランジションも消える
    track.removeClip(b->id());
    QVERIFY(track.transitions().empty());
}

void TestTransitionFilter::transitionProgress()
{
    Transition t;
    t.centerFrame    = 100;
    t.durationFrames = 20;

    QCOMPARE(t.startFrame(), int64_t(90));
    QCOMPARE(t.endFrame(),   int64_t(110));
    QVERIFY(t.contains(90));
    QVERIFY(!t.contains(110));
    QCOMPARE(t.progressAt(90),  0.0f);
    QCOMPARE(t.progressAt(100), 0.5f);
    QCOMPARE(t.progressAt(200), 1.0f);   ///< 区間外はクランプ
}

void TestTransitionFilter::snapshotEmitsPairDuringTransition()
{
    Project project;
    Timeline* tl = project.timeline();
    Track* track = tl->appendTrack(TrackType::Video);
    QVERIFY(track);

    QVERIFY(track->insertClip(makeClip(0, 100, 30, 300)));
    QVERIFY(track->insertClip(makeClip(100, 100, 30, 300)));

    Transition t;
    t.transitionId   = QString::fromLatin1(builtinTransition::kWipe);
    t.centerFrame    = 100;
    t.durationFrames = 20;
    QVERIFY(track->addTransition(t));

    // 区間の外: 1 レイヤー
    RenderSnapshot outside = tl->buildSnapshot(50);
    QCOMPARE(int(outside.layers.size()), 1);
    QVERIFY(!outside.layers[0].transition.has_value());

    // 区間の中: from / to の 2 レイヤーが同じ zIndex で出る (3.10)
    RenderSnapshot inside = tl->buildSnapshot(95);
    QCOMPARE(int(inside.layers.size()), 2);
    QVERIFY(inside.layers[0].transition.has_value());
    QVERIFY(inside.layers[1].transition.has_value());
    QCOMPARE(inside.layers[0].zIndex, inside.layers[1].zIndex);
    QCOMPARE(inside.layers[0].transition->isIncoming, false);
    QCOMPARE(inside.layers[1].transition->isIncoming, true);
    QCOMPARE(inside.layers[0].transition->shaderMode, 2);   ///< wipe
}

// ===========================================================================
//  Undo (3.2)
// ===========================================================================

void TestTransitionFilter::commandsAreUndoable()
{
    Project project;
    Timeline* tl = project.timeline();
    Track* track = tl->appendTrack(TrackType::Video);
    QVERIFY(track);

    auto a = makeClip(0, 100, 30, 300);
    auto b = makeClip(100, 100, 30, 300);
    QVERIFY(track->insertClip(a));
    QVERIFY(track->insertClip(b));

    // --- フィルタ ---
    VideoFilterInstance inst;
    inst.filterId = QString::fromLatin1(builtinFilter::kBlur);
    project.undoStack()->push(new AddFilterCommand(&project, a->id(), inst));
    QCOMPARE(int(a->filters().size()), 1);

    project.undoStack()->undo();
    QCOMPARE(int(a->filters().size()), 0);
    project.undoStack()->redo();
    QCOMPARE(int(a->filters().size()), 1);

    // --- トランジション ---
    Transition t;
    t.transitionId   = QString::fromLatin1(builtinTransition::kDissolve);
    t.centerFrame    = 100;
    t.durationFrames = 20;
    project.undoStack()->push(new AddTransitionCommand(&project, track->id(), t));
    QCOMPARE(int(track->transitions().size()), 1);

    project.undoStack()->undo();
    QCOMPARE(int(track->transitions().size()), 0);
    project.undoStack()->redo();
    QCOMPARE(int(track->transitions().size()), 1);
}

// ===========================================================================
//  シリアライズ (9.3.2 / 9.4.1 / 9.4.6)
// ===========================================================================

void TestTransitionFilter::serializerRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("p.yave"));

    QUuid trackId;
    {
        Project project;
        Timeline* tl = project.timeline();
        Track* track = tl->appendTrack(TrackType::Video);
        QVERIFY(track);
        trackId = track->id();

        auto a = makeClip(0, 100, 30, 300);
        VideoFilterInstance f;
        f.filterId = QString::fromLatin1(builtinFilter::kColorAdjust);
        f.params   = { { QStringLiteral("contrast"), 1.25 } };
        a->addFilter(f);
        QVERIFY(track->insertClip(a));
        QVERIFY(track->insertClip(makeClip(100, 100, 30, 300)));

        Transition t;
        t.transitionId   = QString::fromLatin1(builtinTransition::kSlide);
        t.centerFrame    = 100;
        t.durationFrames = 20;
        QVERIFY(track->addTransition(t));

        auto title = std::make_shared<subtitle::TitleClip>();
        title->applyPreset(QString::fromLatin1(subtitle::builtinTitle::kLowerThird));
        title->setRange({300, 60});
        QVERIFY(track->insertClip(title));

        // フォルダ構成 (9.2.1)
        MediaFolderTree tree;
        MediaFolder folder;
        folder.id   = QUuid::createUuid();
        folder.name = QStringLiteral("Footage");
        tree.folders.push_back(folder);
        project.setMediaFolders(tree);

        QVERIFY(io::ProjectSerializer::save(project, path, {}, nullptr));
    }

    Project loaded;
    const io::LoadResult result = io::ProjectSerializer::load(&loaded, path);
    QVERIFY2(result.ok, qPrintable(result.errorMessage));

    Track* track = loaded.timeline()->trackById(trackId);
    QVERIFY(track);
    QCOMPARE(int(track->clipCount()), 3);
    QCOMPARE(int(track->transitions().size()), 1);
    QCOMPARE(track->transitions()[0].transitionId,
             QString::fromLatin1(builtinTransition::kSlide));
    QCOMPARE(track->transitions()[0].durationFrames, int64_t(20));

    auto first = track->clipAt(0);
    QVERIFY(first);
    QCOMPARE(int(first->filters().size()), 1);
    QCOMPARE(first->filters()[0].params.value(QStringLiteral("contrast")).toDouble(), 1.25);

    auto title = track->clipAt(300);
    QVERIFY(title);
    QCOMPARE(title->type(), ClipType::Title);
    QCOMPARE(static_cast<subtitle::TitleClip*>(title.get())->presetId(),
             QString::fromLatin1(subtitle::builtinTitle::kLowerThird));

    QCOMPARE(int(loaded.mediaFolders().folders.size()), 1);
    QCOMPARE(loaded.mediaFolders().folders[0].name, QStringLiteral("Footage"));
}

QTEST_MAIN(TestTransitionFilter)
#include "tst_transitionfilter.moc"
