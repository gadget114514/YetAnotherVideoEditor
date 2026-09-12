#include "../src/audio/AudioRenderGraph.h"
#include "../src/core/Project.h"
#include "../src/core/Timeline.h"
#include "../src/core/Track.h"
#include "../src/core/AudioClip.h"
#include "../src/core/VideoClip.h"

#include <QtTest/QtTest>

#include <array>

using namespace yave;
using namespace yave::audio;

namespace {

/// 指定 assetId に対して固定の DecodedAudio を返すプロバイダ (5.3.2)。
struct MockProvider : IAudioSourceProvider
{
    QUuid id;
    std::shared_ptr<DecodedAudio> decoded;

    std::shared_ptr<DecodedAudio> decodeAsset(const QUuid& assetId) override
    {
        return assetId == id ? decoded : nullptr;
    }
};

std::shared_ptr<DecodedAudio> makePcm(int64_t frames, int channels)
{
    auto d = std::make_shared<DecodedAudio>();
    d->sampleRate = 48000;
    d->channels   = channels;
    d->channelData.resize(size_t(channels));
    for (int c = 0; c < channels; ++c)
        d->channelData[size_t(c)].assign(size_t(frames), float(c + 1));
    d->channelPtrs.reserve(size_t(channels));
    for (int c = 0; c < channels; ++c)
        d->channelPtrs.push_back(d->channelData[size_t(c)].data());
    return d;
}

AudioClip* addAudioClip(Track* track, const QUuid& assetId,
                        int64_t start, int64_t duration)
{
    auto c = std::make_shared<AudioClip>(assetId);
    c->setRange({start, duration});
    track->insertClip(c);
    return static_cast<AudioClip*>(track->clips().back().get());
}

constexpr float kPanCenterGain = 0.70710678f;

} // anonymous namespace

class TestAudioGraph : public QObject
{
    Q_OBJECT

private slots:
    void providerPopulatesPcm();
    void noProviderLeavesSilence();
    void providerMismatchLeavesSilence();
    void clipSourceSampleConversion();
    void audioOnlyTracksAndClips();
    void decodedAudioPlanarStability();
    void mixBasicStereo();
    void mixMonoToStereo();
    void mixOffsetAndTrim();
    void mixFadeInOut();
    void mixOutsideRangeIsSilent();
};

// ===========================================================================
//  グラフ構築と PCM 供給 (5.3.2)
// ===========================================================================

void TestAudioGraph::providerPopulatesPcm()
{
    Project project;
    project.setSampleRate(48000);
    Timeline* tl = project.timeline();
    Track* t = tl->appendTrack(TrackType::Audio);

    const QUuid assetId = QUuid::createUuid();
    addAudioClip(t, assetId, 0, 10);

    MockProvider provider;
    provider.id      = assetId;
    provider.decoded = makePcm(100, 2);

    auto graph = AudioRenderGraphBuilder::build(*tl, project, &provider);
    QVERIFY(graph != nullptr);
    QCOMPARE(int(graph->tracks.size()), 1);
    QCOMPARE(int(graph->tracks[0].clips.size()), 1);

    const ClipSource& src = graph->tracks[0].clips[0];
    QVERIFY(src.preloadedData != nullptr);
    QCOMPARE(src.preloadedFrames, int64_t(100));
    QCOMPARE(src.channels, 2);
    QCOMPARE(int(graph->ownedAudio.size()), 1);
    // グラフが供給バッファを所有 (shared_ptr で寿命管理) していること
    QVERIFY(graph->ownedAudio[0] == provider.decoded);
}

void TestAudioGraph::noProviderLeavesSilence()
{
    Project project;
    Timeline* tl = project.timeline();
    Track* t = tl->appendTrack(TrackType::Audio);
    addAudioClip(t, QUuid::createUuid(), 0, 10);

    auto graph = AudioRenderGraphBuilder::build(*tl, project, nullptr);
    QVERIFY(graph != nullptr);
    QCOMPARE(int(graph->tracks.size()), 1);
    QCOMPARE(int(graph->tracks[0].clips.size()), 1);
    QVERIFY(graph->tracks[0].clips[0].preloadedData == nullptr);
    QVERIFY(graph->ownedAudio.empty());
}

void TestAudioGraph::providerMismatchLeavesSilence()
{
    Project project;
    Timeline* tl = project.timeline();
    Track* t = tl->appendTrack(TrackType::Audio);
    addAudioClip(t, QUuid::createUuid(), 0, 10);

    // プロバイダは別の assetId を持つ -> 一致しないので無音のまま
    MockProvider provider;
    provider.id      = QUuid::createUuid();
    provider.decoded = makePcm(10, 2);

    auto graph = AudioRenderGraphBuilder::build(*tl, project, &provider);
    QVERIFY(graph->tracks[0].clips[0].preloadedData == nullptr);
    QVERIFY(graph->ownedAudio.empty());
}

void TestAudioGraph::clipSourceSampleConversion()
{
    Project project;
    project.setSampleRate(48000);
    Timeline* tl = project.timeline();
    Track* t = tl->appendTrack(TrackType::Audio);

    auto clip = std::make_shared<AudioClip>();
    clip->setRange({100, 50});
    clip->setSourceOffset(20);
    clip->setGain(0.8);
    clip->setPan(-0.5);
    clip->setFadeInFrames(10);
    clip->setFadeOutFrames(5);
    QVERIFY(t->insertClip(clip));

    auto graph = AudioRenderGraphBuilder::build(*tl, project, nullptr);
    const ClipSource& src = graph->tracks[0].clips[0];

    // フレーム -> サンプル換算 (プロジェクト sampleRate / タイムベース)
    const Rational tb = tl->timebase();
    const double f2s  = 48000.0 / tb.toDouble();
    QCOMPARE(src.timelineStart, int64_t(double(100) * f2s));
    QCOMPARE(src.timelineEnd,   int64_t(double(150) * f2s));
    QCOMPARE(src.sourceOffset,  int64_t(double(20)  * f2s));
    QCOMPARE(src.fadeInSamples, int64_t(double(10)  * f2s));
    QCOMPARE(src.fadeOutSamples,int64_t(double(5)   * f2s));
    QCOMPARE(src.gain, 0.8f);
    QCOMPARE(src.pan, -0.5f);
}

void TestAudioGraph::audioOnlyTracksAndClips()
{
    Project project;
    Timeline* tl = project.timeline();
    Track* v = tl->appendTrack(TrackType::Video);
    tl->appendTrack(TrackType::Audio);
    tl->appendTrack(TrackType::Subtitle);

    // ビデオトラック上のビデオクリップはグラフへ載らない
    auto vc = std::make_shared<VideoClip>();
    vc->setRange({0, 10});
    QVERIFY(v->insertClip(vc));

    Track* a = tl->trackAt(1);
    addAudioClip(a, QUuid::createUuid(), 0, 10);

    auto graph = AudioRenderGraphBuilder::build(*tl, project, nullptr);
    QCOMPARE(int(graph->tracks.size()), 1);           ///< 音声トラックのみ
    QCOMPARE(int(graph->tracks[0].clips.size()), 1);  ///< 音声クリップのみ
}

void TestAudioGraph::decodedAudioPlanarStability()
{
    auto d = makePcm(64, 2);
    QCOMPARE(d->frames(), int64_t(64));
    QCOMPARE(d->channels, 2);
    QCOMPARE(int(d->channelPtrs.size()), 2);
    QVERIFY(d->planar() == d->channelPtrs.data());
    QCOMPARE(d->channelPtrs[0], d->channelData[0].data());
    QCOMPARE(d->channelPtrs[1], d->channelData[1].data());
    QCOMPARE(d->planar()[0][63], 1.0f);
    QCOMPARE(d->planar()[1][63], 2.0f);
}

// ===========================================================================
//  ミックス (mixClipBlock)
// ===========================================================================

void TestAudioGraph::mixBasicStereo()
{
    auto d = makePcm(64, 2);
    ClipSource src;
    src.preloadedData   = d->planar();
    src.preloadedFrames = d->frames();
    src.channels        = 2;
    src.timelineStart   = 0;
    src.timelineEnd     = 64;
    src.gain            = 1.0f;
    src.pan             = 0.0f;

    std::array<float, 64> l{};
    std::array<float, 64> r{};
    float* buf[2] = { l.data(), r.data() };
    mixClipBlock(buf, 2, 64, 0, src);

    // パン中央: 各チャンネルへ等価パワーで分配される
    QVERIFY(qAbs(l[0]  - kPanCenterGain * 1.0f) < 1e-6f);
    QVERIFY(qAbs(r[0]  - kPanCenterGain * 2.0f) < 1e-6f);
    QVERIFY(qAbs(l[63] - kPanCenterGain * 1.0f) < 1e-6f);
    QVERIFY(qAbs(r[63] - kPanCenterGain * 2.0f) < 1e-6f);
}

void TestAudioGraph::mixMonoToStereo()
{
    auto d = makePcm(64, 1);
    ClipSource src;
    src.preloadedData   = d->planar();
    src.preloadedFrames = d->frames();
    src.channels        = 1;
    src.timelineStart   = 0;
    src.timelineEnd     = 64;
    src.gain            = 1.0f;
    src.pan             = 0.0f;

    std::array<float, 64> l{};
    std::array<float, 64> r{};
    float* buf[2] = { l.data(), r.data() };
    mixClipBlock(buf, 2, 64, 0, src);

    // モノラル中央: 両チャンネルへ等分配
    QVERIFY(qAbs(l[0] - kPanCenterGain) < 1e-6f);
    QVERIFY(qAbs(r[0] - kPanCenterGain) < 1e-6f);

    // 完全左パン: 左だけ 1.0
    ClipSource hardLeft = src;
    hardLeft.pan = -1.0f;
    std::array<float, 64> l2{};
    std::array<float, 64> r2{};
    float* buf2[2] = { l2.data(), r2.data() };
    mixClipBlock(buf2, 2, 64, 0, hardLeft);
    QVERIFY(qAbs(l2[0] - 1.0f) < 1e-6f);
    QVERIFY(qAbs(r2[0]) < 1e-6f);
}

void TestAudioGraph::mixOffsetAndTrim()
{
    auto d = makePcm(64, 2);
    ClipSource src;
    src.preloadedData   = d->planar();
    src.preloadedFrames = d->frames();
    src.channels        = 2;
    // クリップ区間 [16, 32) サンプル、ソースオフセット 4
    src.timelineStart   = 16;
    src.timelineEnd     = 32;
    src.sourceOffset    = 4;
    src.gain            = 1.0f;
    src.pan             = 0.0f;

    std::array<float, 64> l{};
    std::array<float, 64> r{};
    float* buf[2] = { l.data(), r.data() };

    // ブロック [20, 28) はクリップ中央の一部。ソース位置 = 4 + (20-16) = 8
    mixClipBlock(buf, 2, 8, 20, src);
    QVERIFY(qAbs(l[0] - kPanCenterGain * 1.0f) < 1e-6f);
    QVERIFY(qAbs(l[7] - kPanCenterGain * 1.0f) < 1e-6f);

    // クリップ区間外の出力位置には何も足されない
    QCOMPARE(l[8], 0.0f);
    QCOMPARE(r[8], 0.0f);
}

void TestAudioGraph::mixFadeInOut()
{
    auto d = makePcm(32, 2);
    ClipSource src;
    src.preloadedData    = d->planar();
    src.preloadedFrames  = d->frames();
    src.channels         = 2;
    src.timelineStart    = 0;
    src.timelineEnd      = 32;
    src.fadeInSamples    = 8;
    src.fadeOutSamples   = 8;
    src.gain             = 1.0f;
    src.pan              = 0.0f;

    std::array<float, 32> l{};
    std::array<float, 32> r{};
    float* buf[2] = { l.data(), r.data() };
    mixClipBlock(buf, 2, 32, 0, src);

    // フェードイン: i=0 -> 0, i=7 -> 7/8
    QVERIFY(qAbs(l[0]) < 1e-6f);
    QVERIFY(qAbs(l[7] - kPanCenterGain * (7.0f / 8.0f)) < 1e-6f);
    // 中間はフル
    QVERIFY(qAbs(l[16] - kPanCenterGain) < 1e-6f);
    // フェードアウト: i=24 -> 1, i=31 -> 1/8
    QVERIFY(qAbs(l[24] - kPanCenterGain) < 1e-6f);
    QVERIFY(qAbs(l[31] - kPanCenterGain * (1.0f / 8.0f)) < 1e-6f);
}

void TestAudioGraph::mixOutsideRangeIsSilent()
{
    auto d = makePcm(32, 2);
    ClipSource src;
    src.preloadedData   = d->planar();
    src.preloadedFrames = d->frames();
    src.channels        = 2;
    src.timelineStart   = 0;
    src.timelineEnd     = 32;

    std::array<float, 64> l{};
    std::array<float, 64> r{};
    float* buf[2] = { l.data(), r.data() };

    // ブロックがクリップより前
    mixClipBlock(buf, 2, 16, -32, src);
    QCOMPARE(l[0], 0.0f);
    // ブロックがクリップより後
    mixClipBlock(buf, 2, 16, 32, src);
    QCOMPARE(l[0], 0.0f);
}

QTEST_MAIN(TestAudioGraph)
#include "tst_audiograph.moc"