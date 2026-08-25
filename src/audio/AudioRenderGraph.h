#pragma once

#include "../core/Rational.h"
#include "DelayCompensator.h"

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
};

/// AudioRenderGraph の構築ヘルパ (UI スレッドから呼ぶ)。
///
/// 音声データの事前デコード方針 (5.3.2):
///   RT スレッド内でデコードはできない (malloc とディスク I/O が発生する)。
///   再生範囲の音声はあらかじめメモリに載せるか、ページ単位のストリーミング
///   キャッシュ (AudioStreamCache, 将来実装) から供給する。
/// 現行実装では PCM 参照を持たないグラフを構築する (無音出力)。
class AudioRenderGraphBuilder
{
public:
    /// Timeline -> AudioRenderGraph。
    static std::unique_ptr<AudioRenderGraph> build(const yave::Timeline& timeline,
                                                   const yave::Project& project);
};

} // namespace yave::audio
