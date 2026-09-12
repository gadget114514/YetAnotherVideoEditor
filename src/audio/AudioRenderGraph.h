#pragma once

#include "../core/Rational.h"
#include "DelayCompensator.h"

#include <QUuid>

#include <cstdint>
#include <memory>
#include <vector>

namespace yave {

class Timeline;
class Project;
class Track;
class IAudioEffectNode;

} // namespace yave

namespace yave::audio {

/// 事前デコードされた float PCM 音声。グラフが所有権を持ち、RT スレッドが参照する。
///
/// グラフ構築後はイミュータブルとして扱う。channelPtrs は channelData を
/// 埋め終えた後に一度だけ構築し、以後変化させない (アドレス安定)。
struct DecodedAudio
{
    int    sampleRate = 0;
    int    channels   = 0;
    std::vector<std::vector<float>> channelData;   ///< [ch][sample]
    std::vector<const float*>       channelPtrs;   ///< channelData[i].data() のスナップショット

    /// RT スレッドが読む ClipSource::preloadedData へ渡す配列。
    const float* const* planar() const { return channelPtrs.data(); }
    int64_t frames() const { return channelData.empty() ? 0 : int64_t(channelData[0].size()); }
};

/// 音声ソース (素材) の供給インタフェース。
/// UI スレッドのグラフ構築時に呼ばれる。デコード実体は app / media 層が提供する。
class IAudioSourceProvider
{
public:
    virtual ~IAudioSourceProvider() = default;

    /// assetId の音声を float PCM へデコードして返す。失敗 / 未対応は nullptr。
    virtual std::shared_ptr<DecodedAudio> decodeAsset(const QUuid& assetId) = 0;
};

/// RT スレッドが読む POD グラフ。Timeline とは完全に分離されたデータ構造。
///
/// スレッド安全性: publish 後はイミュータブルとして扱う。
/// 差し替えは RCU (std::atomic<AudioRenderGraph*>) で行い、
/// 古いグラフは RT が 2 世代進んでから破棄する。
struct ClipSource            ///< 再生すべき音声クリップ 1 個分
{
    int64_t timelineStart = 0;    ///< サンプル単位
    int64_t timelineEnd   = 0;    ///< サンプル単位 (排他)
    int64_t sourceOffset  = 0;    ///< ソース内の開始サンプル
    float   gain          = 1.0f;
    float   pan           = 0.0f;

    /// 事前デコード済み PCM (チャンネル配列へのポインタ配列)。null なら無音。
    const float* const* preloadedData = nullptr;
    int64_t preloadedFrames = 0;
    int     channels        = 2;

    // フェード (サンプル単位)
    int64_t fadeInSamples  = 0;
    int64_t fadeOutSamples = 0;
};

/// 1 クリップ分の PCM を出力バッファへ加算ミックスする (RT スレッドから呼ぶ)。
///
/// buf は事前にゼロ初期化済みであること。blockStart..blockStart+numFrames が
/// クリップ区間と重ならない場合、または PCM を持たないクリップは何もしない。
/// ソースのチャンネル数が出力より少ない場合はパンで分配し、多い場合は切り詰める。
void mixClipBlock(float* const* buf, int channels, int numFrames,
                  int64_t blockStart, const ClipSource& src) noexcept;

struct TrackNode
{
    std::vector<ClipSource> clips;      ///< timelineStart 昇順
    float gain  = 1.0f;
    float pan   = 0.0f;
    bool  muted = false;
    bool  solo  = false;

    std::vector<yave::IAudioEffectNode*> effectChain;   ///< 所有はしない
    int64_t chainLatencySamples = 0;                    ///< PDC 用の合計レイテンシ
    int64_t compensationDelay   = 0;                    ///< このトラックに追加すべき遅延

    /// compensationDelay 用リングバッファ。UI スレッドが prepare() 済みのものを指す。
    DelayLine* delayLine = nullptr;
};

struct AudioRenderGraph
{
    int sampleRate     = 48000;
    int maxBlockFrames = 512;
    std::vector<TrackNode> tracks;
    float masterGain = 1.0f;
    std::vector<yave::IAudioEffectNode*> masterChain;
    int64_t masterLatency = 0;
    bool    anySolo = false;

    // ループ再生
    bool    loopEnabled   = false;
    int64_t loopStartSample = 0;
    int64_t loopEndSample   = 0;

    /// このグラフが所有する事前デコードバッファ。
    /// ClipSource::preloadedData はここが保持する DecodedAudio を指す。
    std::vector<std::shared_ptr<DecodedAudio>> ownedAudio;
};

/// AudioRenderGraph の構築ヘルパ (UI スレッドから呼ぶ)。
///
/// 音声データの事前デコード方針 (5.3.2):
///   RT スレッド内でデコードはできない (malloc とディスク I/O が発生する)。
///   再生範囲の音声はあらかじめメモリに載せるか、ページ単位のストリーミング
///   キャッシュ (AudioStreamCache, 将来実装) から供給する。
/// sources が非 null の場合、各音声クリップの PCM をここから引き、
/// ClipSource::preloadedData へ参照を張る。
class AudioRenderGraphBuilder
{
public:
    /// Timeline -> AudioRenderGraph。
    static std::unique_ptr<AudioRenderGraph> build(const yave::Timeline& timeline,
                                                   const yave::Project& project,
                                                   IAudioSourceProvider* sources = nullptr);
};

} // namespace yave::audio
