# 4. ビデオエンジン (WebCodecs + GPU)

[← 目次に戻る](../design.md)

---

## 4.1 方針

- **WebCodecs を第一級のデコード / エンコード基盤**とする。ブラウザのハードウェアデコーダを活用し、低遅延かつ高効率なフレーム取得を実現する。
- **ffmpeg.wasm はプロキシメディア生成専用**に限定する。リアルタイムプレビューの主経路に WASM ソフトウェアデコーダを通すと 1080p60 のフレーム予算を維持できないため。
- **Electron 版では同梱 ffmpeg バイナリによるネイティブ書き出し**をオプションとして提供し、長尺動画の高速エンコードや多様なコーデックに対応する。

> **Web 版の制約**:
> ブラウザの WebCodecs は OS / ブラウザが対応するコーデック (H.264, VP9, AV1, HEVC ※環境依存) に制限され、ProRes, DNxHD 等の編集用中間コーデックは直接デコードできない。これらの素材が投入された場合は、`ffmpeg.wasm` を用いて 540p H.264 のプロキシ動画を OPFS 内に自動生成して編集を行う。Electron 版ではネイティブ ffmpeg により中間コーデックの直接読み込みが可能。

---

## 4.2 デマックス (Demux)

メディアコンテナの解析とパケット分離は、メインスレッドをブロックしないよう `DecodeWorker` 内で行う。

| コンテナ | 採用ライブラリ | 備考 |
|---|---|---|
| MP4 / MOV | **mp4box.js** | moov/trak/stbl の解析、AVC/HEVC サンプルの抽出 |
| WebM / MKV | **web-demuxer** (または ebml.js) | Matroska / WebM の VP8/VP9/AV1/Opus サンプル抽出 |
| WAV / MP3 / AAC | 自前ヘッダパーサ / Web Audio API | 単体音声ファイル |

---

## 4.3 デコード

### 4.3.1 VideoDecoder の初期化と HW 判定

```ts
// packages/engine/src/media/workers/decode.worker.ts
export async function initializeDecoder(config: VideoDecoderConfig): Promise<VideoDecoder> {
  const support = await VideoDecoder.isConfigSupported({
    ...config,
    hardwareAcceleration: 'prefer-hardware',
  });

  if (!support.supported) {
    throw new Error(`Unsupported video format: ${config.codec}`);
  }

  const decoder = new VideoDecoder({
    output: (frame: VideoFrame) => {
      // デコード完了コールバック
      handleDecodedFrame(frame);
    },
    error: (e: DOMException) => {
      console.error('VideoDecoder error:', e);
    },
  });

  decoder.configure({
    ...config,
    hardwareAcceleration: 'prefer-hardware',
  });

  return decoder;
}
```

### 4.3.2 精密シーク (Accurate Seek)

動画の In 点やランダムシーク位置は、必ずしもキーフレーム (IDR / I-Frame) に一致しない。目的フレーム $F_{\text{target}}$ を正確に表示するため、以下のアルゴリズムでシークを行う。

```
1. demuxer のインデックスを参照し、F_target 直前の最新キーフレーム F_key を特定
2. decoder.reset() を実行して内部バッファをクリア
3. F_key から F_target までの全パケットを順次 decoder.decode(chunk) に投入
4. output コールバックに渡される VideoFrame のタイムスタンプ (timestamp) を確認
   - timestamp < F_target のフレーム: 即座に frame.close() を呼んで破棄
   - timestamp == F_target のフレーム: キャッシュに格納し、メインスレッドへ転送
5. decoder.flush() を待機してシーク完了
```

---

## 4.4 `VideoFrame` のライフサイクル管理

`VideoFrame` は GPU / システムのネイティブメモリを直接参照するオブジェクトである。JavaScript のガベージコレクションに依存すると、短時間で VRAM が枯渇してブラウザタブがクラッシュする。

### ライフサイクル規則
1. **所有権の明示**: フレームを保持するオブジェクト (`FrameCache`) が唯一の所有者となる。
2. **Transferable 転送**: Worker からメインスレッドへ送る際は `postMessage([frame], [frame])` を使い、Worker 側の参照を即座に無効化する。
3. **明示的破棄**: 使用が終わったフレームは、次フレーム描画完了時またはキャッシュ追い出し時に必ず `frame.close()` を呼ぶ。
4. **クローン時の注意**: 複数レイヤーで同一フレームを参照する場合は `frame.clone()` を使用し、クローン側も個別に `close()` する。
5. **リーク検出**: 開発モードでは生成数と `close()` 呼び出し数のカウンタを保持し、差分が 60 枚を超えた場合に警告を出力する。

---

## 4.5 フレームキャッシュと先読み

```
               再生ヘッド (Playhead)
                       │
       ┌───────────────┼───────────────────────────┐
       ▼               ▼                           ▼
   過去 10 フレーム    現在フレーム          先行 20 フレーム
   [ Cache (LRU) ]    [ Rendering ]         [ Look-ahead Prefetch ]
```

- **キャッシュ上限**: 既定 60 枚 (1080p で約 480MB)。メモリ制限環境では 30 枚に自動調整。
- **再生方向の Look-ahead**: 再生速度が等速正方向の場合、DecodeWorker は再生ヘッドの 0.3〜0.5 秒先までのパケットを先回りしてデコードし、メインスレッドへ push する。

---

## 4.6 プロキシメディア (Proxy)

4K 解像度や高ビットレート素材、WebCodecs 非対応形式の素材に対しては、軽量な編集用プロキシを生成する。

- **解像度**: 960x540 (1/4 解像度)
- **コーデック**: H.264 Baseline / Stereo AAC
- **生成主体**: `ffmpeg.wasm` をバックグラウンド Worker で実行
- **保存先**: OPFS (Origin Private File System) の `/yave/proxy/<assetId>.mp4`
- **シームレス切替**: 編集・再生中はプロキシテクスチャを参照し、最終書き出し (`ExportJob`) 時に元素材を参照する。

---

## 4.7 書き出し (Export)

書き出しはリアルタイム再生とは独立した `ExportJob` パイプラインで処理する。

```
 [ExportJob]
   1. 出力先ストリームの作成 (PlatformHost.createExportSink)
   2. VideoEncoder / AudioEncoder の初期化
      - H.264 (avc1.640028) / AAC (mp4a.40.2)
      - ビットレート、プリセット、GOP サイズの設定
   3. mp4-muxer の初期化
   4. フレームごとのループ (フレーム 0 から N-1):
      - オフライン WebGL2 FBO で canvasSize によるフル品質合成 (フィルタ・トランジション含む)
      - FBO から VideoFrame を生成 (new VideoFrame(fboCanvas, { timestamp }))
      - videoEncoder.encode(frame, { keyFrame: i % 60 === 0 })
      - frame.close()
      - 同フレームの音声を mixClips で評価し audioEncoder.encode(audioData)
      - 進捗コールバックを発行 (onProgress(i / N))
   5. videoEncoder.flush(), audioEncoder.flush()
   6. mp4-muxer.finalize()
   7. 出力ストリームを close()
```

### 対応コーデック・コンテナ

| 環境 | コンテナ | 映像コーデック | 音声コーデック |
|---|---|---|---|
| Web (標準) | MP4 | H.264 (AVC) | AAC |
| Web (オープン) | WebM | VP9 / AV1 | Opus |
| Electron | MP4 / WebM / MOV | H.264, H.265, ProRes, VP9, AV1 | AAC, Opus, Linear PCM |

---

## 4.8 カラーマネジメント

- **色空間**: `VideoFrame.colorSpace` を参照。原則として **Rec.709 (BT.709)** フルレンジをターゲットとして合成する。
- **YUV to RGB**: WebGL2 のフラグメントシェーダ内で標準 Rec.709 変換行列を用いて RGB 空間へマッピングする。
- **HDR**: Web 版初期リリースでは HDR (BT.2020 / PQ / HLG) は対象外とし、SDR (Rec.709) へのトーンマップまたは SDR 素材前提とする。
