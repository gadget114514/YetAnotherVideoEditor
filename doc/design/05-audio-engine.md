# 5. オーディオエンジン

[← 目次に戻る](../design.md)

---

## 5.1 オーディオファースト同期

### 5.1.1 基本原則

**AudioWorklet の音声サンプルカウンタがタイムライン全体のマスタークロックである。**
映像の描画ループは常にこのオーディオクロックに追従する。映像タイマー駆動で音を合わせる逆方向の制御は行わない。

> **理由**:
> 人間の聴覚は音の途切れやピッチのジッターに対して非常に敏感である。また、OS やブラウザのディスプレイリフレッシュレート (60Hz, 120Hz, 144Hz 等) とオーディオハードウェアの水晶発振器クロック (48kHz) は非同期に動作しており、ブラウザのタイマーや rAF で時間を刻んでも必ずズレが生じる。オーディオデバイスの再生位置を基準とし、映像側が「現在再生中のサンプル位置に最も近い映像フレームを表示する」構造とすることで、安定した音映像同期を維持する。

### 5.1.2 クロックの伝達

```
 [AudioWorklet Thread] (128 samples / quantum @ 48kHz)
   MixerWorklet.process()
        │
   AudioRenderGraph を評価して各トラック PCM をミックス
        │
   playedSamples += 128
   Atomics.store(sharedSampleArray, 0, playedSamples)
        │
        ▼ (SharedArrayBuffer)
 [Main Thread - rAF ループ]
   Atomics.load(sharedSampleArray, 0)
        │
   playedSamples から出力デバイス遅延を引いて、現在耳に届いているサンプル位置を算出:
     audibleSample = playedSamples - latencySamples
     seconds = audibleSample / sampleRate
     currentFrame = secondsToFrames(seconds, projectTimebase, 'nearest')
        │
        ▼
   Timeline.buildSnapshot(currentFrame) -> WebGlCompositor.renderFrame()
```

> **cross-origin isolation が無効な場合のフォールバック**:
> 何らかの理由で `SharedArrayBuffer` が利用できない環境では、`AudioContext.getOutputTimestamp()` を用いてコンテキスト時間から再生位置を推定する。

---

## 5.2 AudioEngine 構成

Web Audio API の `AudioContext` をベースとし、カスタム `AudioWorkletNode` をミキサーのコアとして配置する。

```
 [AudioContext] (48,000Hz)
       │
   [MixerWorkletNode] ◄── port.postMessage(AudioRenderGraph)
       │ (2ch stereo)
   [GainNode (Master Gain)]
       │
   [AudioDestinationNode (Speakers / Headphones)]
```

---

## 5.3 AudioRenderGraph の差し替え

タイムラインが編集された際、メインスレッドは新しい音声グラフ記述 (`AudioRenderGraph`) を構築し、Worklet の MessagePort を介して転送する。Worklet 側は受信した新しいグラフを次の量子化処理 (128 サンプル境界) でアトミックに差し替える。

```ts
export interface AudioTrackDescriptor {
  readonly trackId: string;
  readonly gain: number;
  readonly pan: number;
  readonly muted: boolean;
  readonly solo: boolean;
  readonly clips: readonly {
    readonly clipId: string;
    readonly startSample: number;
    readonly durationSamples: number;
    readonly sourceOffsetSamples: number;
    readonly pcmBufferId: string;
    readonly gain: number;
  }[];
  readonly effectChain: readonly any[];
  readonly compensationDelaySamples: number;
}

export interface AudioRenderGraphDescriptor {
  readonly tracks: readonly AudioTrackDescriptor[];
  readonly masterGain: number;
  readonly anySolo: boolean;
}
```

---

## 5.4 音声データの事前デコード

リアルタイムの AudioWorklet スレッド内でディスクアクセスやデコード処理を行うことはできない。そのため、**編集中のタイムライン範囲の音声データは事前にデコードしてメモリに保持**する。

1. **デコード**: WebCodecs `AudioDecoder` を Worker 内で実行し、圧縮音声 (AAC/Opus/MP3) を Float32 PCM 配列 (`AudioBuffer` / `Float32Array`) に展開。
2. **リサンプリング**: プロジェクトのサンプリングレート (48kHz) と素材のレートが異なる場合は、Worker 内で線形または sinc リサンプリングを実施。
3. **共有**: デコード済み PCM は `AudioClip` の assetId と紐づけて Worklet または SharedArrayBuffer 上のメモリプールに登録。

---

## 5.5 プラグイン遅延補正 (PDC)

エフェクトや将来の WAM (Web Audio Modules) プラグインがルックアヘッドや FFT 処理のために固有の処理遅延を持つ場合、トラック間で音声のタイミングがずれる。これを相殺するため PDC アルゴリズムを適用する。

### 5.5.1 補正アルゴリズム

```
1. 各オーディオトラック t について、チェーン内エフェクトの申告レイテンシを合計:
     chainLatency[t] = Σ fx.latencySamples

2. マスターチェーンのレイテンシも算出:
     masterLatency = Σ masterFx.latencySamples

3. 全トラック中の最大レイテンシを算出:
     maxLatency = max(chainLatency[t]) (for all tracks t)

4. 各トラックに挿入すべき遅延量を決定:
     compensationDelay[t] = maxLatency - chainLatency[t]

5. 映像と音声を全体で一致させるため、映像描画の参照サンプル位置を前倒し:
     totalPluginLatency = maxLatency + masterLatency
     audibleSample = playedSamples - latencySamples - totalPluginLatency
```

Worklet 内の各トラック処理ノードは、`compensationDelay[t]` 分の循環遅延リングバッファ (`DelayLine`) を通してからマスターミックスへ加算する。

---

## 5.6 オフラインレンダリング

最終動画の書き出し (`ExportJob`) においては、リアルタイム再生を待たずに最高速で音声を合成する必要がある。

- **方式**: `OfflineAudioContext` を使用するか、Worklet と同一のミックス処理関数を Worker 内でループ実行して書き出し用の PCM バッファを一括生成する。
- 生成された Float32 PCM は `AudioEncoder` に渡され、AAC / Opus としてエンコードされる。

---

## 5.7 波形表示 (Waveform)

タイムライン上の音声クリップにオーディオ波形を表示するため、Worker でピークデータを事前計算する。

1. 音声素材がインポートされた際、`waveform.worker.ts` が起動。
2. 音声 PCM を読み込み、一定サンプル数 (例: 256 サンプルごと) の最大値・最小値 (min/max ピーク対) を計算。
3. 計算結果の Float32Array を OPFS (`/yave/waveform/<assetId>.bin`) にバイナリキャッシュ。
4. UI のタイムライン描画コンポーネントは、Canvas2D のパス描画 (`ctx.lineTo`) を用いて波形を描画する。
