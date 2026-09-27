# 14. 移行ガイド & テスト対応表

[← 目次に戻る](../design.md)

---

## 14.1 旧 (Qt/C++) → 新 (TypeScript/React) モジュール対応表

| 旧モジュール (C++ / Qt) | 新パッケージ (TypeScript / React) | 移行内容 |
|---|---|---|
| `src/core` | `packages/core` | `Rational`, `TimeRange`, `Clip` 派生群, `Track`, `Timeline`, `Project`, `CommandStack`, Undo コマンド群 |
| `src/io` | `packages/io` | `ProjectSerializer`, `SchemaMigration`, `JsonKeys`, `EnumMapping`, `PathResolver` |
| `src/subtitle` | `packages/core` (モデル) + `packages/io/srt` (パーサ) | `SubtitleClip`, `SubtitleStyle`, `SrtParser`, `SrtWriter` |
| `src/media` | `packages/engine/media` | WebCodecs `VideoDecoder`/`AudioDecoder`, mp4box.js, web-demuxer, `FrameCache`, `ExportJob` |
| `src/render` | `packages/engine/render` | WebGL2 `WebGlCompositor`, GLSL ES 3.0, `TexturePool`, `FboPool` |
| `src/audio` + `src/platform/win/Wasapi*` | `packages/engine/audio` | Web Audio API `AudioEngine`, `AudioWorklet`, `AudioClock`, `DelayCompensator` (PDC) |
| `src/ai` | `packages/engine/ai` | `AiOrchestrator`, `OnnxWebProvider`, `RemoteHttpProvider`, `GenerationCache` |
| `src/plugin` | `packages/engine/plugins` | ES module JS プラグインローダ (VST3 / AviUtl は廃止) |
| `src/app/qml` | `packages/ui` | React 18 コンポーネント群, flexlayout-react パネル群, CSS Modules |
| `src/app/controllers` | `packages/ui/stores` | Zustand stores (`useProjectStore`, `usePlaybackStore`, `useSelectionStore`) |
| `src/i18n` | `locales/*.json` + `packages/ui/i18n` | i18next, `useLanguage` hook, `ja.json`, `en.json` |
| `src/launcher` | — | 不要 (Electron の main プロセスが代替) |
| `include/yave/sdk` | `packages/plugin-sdk` | サードパーティ向け JS プラグイン公開 API 型定義 |

---

## 14.2 廃止機能一覧と理由

| 廃止機能 | 理由 | 代替策 |
|---|---|---|
| **VST3 プラグインホスト** | ブラウザ環境でネイティブコード (.vst3) を実行できず、クロスプラットフォーム可搬性を損なうため | 将来の **WAM (Web Audio Modules) 2.0** 対応 |
| **AviUtl プラグインホスト** | Windows 専用の x64 ネイティブ DLL であり、Web および macOS で動作しないため | ES module による **JS 映像フィルタ / トランジションプラグイン** |
| **WASAPI 排他モード** | OS 依存の低遅延オーディオ API のため | Web Audio API + **AudioWorklet** (低遅延コールバック) |
| **D3D11 ゼロコピー描画** | Direct3D 11 ネイティブテクスチャ共有のため | WebGL2 + **`VideoFrame` 外部テクスチャバインド / `texImage2D`** |
| **C ABI ネイティブプラグイン** | コンパイラ・プラットフォーム依存のバイナリのため | **TypeScript / JavaScript ES module プラグイン** (`import()`) |
| **HDR (BT.2020 / PQ / HLG)** | ブラウザのカラーマネジメント差異が大きく品質保証が困難なため | **SDR (Rec.709)** を基準色空間として統一 |

---

## 14.3 単体テスト移植表

旧 C++ 単体テスト (Qt Test) 全 11 本は、Vitest による TypeScript 単体テストとして `packages/*/test/*.test.ts` へ 1:1 で完全移植する。

| 旧テストファイル | 新テストファイル | 移植対象テストケース一覧 |
|---|---|---|
| `tests/tst_rational.cpp` | `packages/core/test/rational.test.ts` | `reduced`, `comparison`, `overflowComparison`, `arithmetic`, `secondsToFrames`, `framesToSeconds`, `rescaleFrames`, `dropFrameTimebase` |
| `tests/tst_timeline.cpp` | `packages/core/test/timeline.test.ts` | `trackInsertRemove`, `clipOverlapRejection`, `clipAtBinarySearch`, `clipsIn`, `moveTrackZOrder`, `snapshotBuilding`, `splitCommandUndoRedo`, `rippleDelete`, `undoStackIntegration` |
| `tests/tst_srtparser.cpp` | `packages/io/test/srtparser.test.ts` | `parseBasic`, `parseShiftJisFallback`, `parseInlineMarkup`, `convertCuesToClipsRounding`, `writeRoundTrip`, `malformedInput` |
| `tests/tst_subtitlecommands.cpp` | `packages/core/test/subtitlecommands.test.ts` | `importSrtBulk`, `importSrtOverlapPolicy`, `importUndoRestores`, `editText`, `editTextMerge`, `setStyleField`, `setStyleUndo` |
| `tests/tst_projectserializer.cpp` | `packages/io/test/projectserializer.test.ts` | `saveLoadRoundTrip`, `unknownFieldsPreserved`, `atomicSaveKeepsOriginal`, `enumStringsInJson`, `schemaVersionWarning`, `autosaveCompact`, `dndAudioAssetRoundTrip`, `dndSrtImportRoundTrip` |
| `tests/tst_delaycompensator.cpp` | `packages/engine/test/delaycompensator.test.ts` | `computeBasic`, `zeroLatency`, `masterChainOnly`, `delayLineRoundTrip`, `delayLinePowerOfTwoCapacity` |
| `tests/tst_i18n.cpp` | `packages/ui/test/i18n.test.ts` | `switchLanguageAtRuntime`, `timecodeIsLocaleIndependent` |
| `tests/tst_transitionfilter.cpp` | `packages/core/test/transitionfilter.test.ts` | `filterStackOrder`, `filterParamsResolve`, `transitionNeedsBoundary`, `transitionClampedToHandles`, `transitionRejectedWithoutHandles`, `transitionDroppedWhenClipRemoved`, `transitionProgress`, `snapshotEmitsPairDuringTransition`, `commandsAreUndoable`, `serializerRoundTrip` |
| `tests/tst_audiograph.cpp` | `packages/engine/test/audiograph.test.ts` | `providerPopulatesPcm`, `noProviderLeavesSilence`, `providerMismatchLeavesSilence`, `clipSourceSampleConversion`, `audioOnlyTracksAndClips`, `decodedAudioPlanarStability`, `mixBasicStereo`, `mixMonoToStereo`, `mixOffsetAndTrim`, `mixFadeInOut`, `mixOutsideRangeIsSilent` |
| `tests/tst_propertycommands.cpp` | `packages/core/test/propertycommands.test.ts` | `clipGainPanUndoRedo`, `clipOpacityBlendUndoRedo`, `clipRangeNameFade`, `trackGainMutedUndoRedo`, `trackOpacityBlendUndoRedo`, `trackNameHeightVisible` |
| `tests/tst_mediaprobe.cpp` | `packages/engine/test/mediaprobe.test.ts` | `audioDurationIsSampleCount`, `stereoChannelsAndRate`, `missingFileFails` |

---

## 14.4 移行マイルストーン

| マイルストーン | 内容 | 完了条件 |
|---|---|---|
| **M0** | リポジトリ雛形 (pnpm workspace, Vite, Electron, CI, GitHub Pages) | 空の画面が GitHub Pages と Electron で起動・表示 |
| **M1** | core + io (Rational, Timeline, Commands, Serializer, SRT) | 移植テスト (rational, timeline, srtparser, serializer 等) 全緑、旧 `sample_project.yave` の読み込み成功 |
| **M2** | UI 骨格 (flexlayout-react 8 パネル, ライブラリ, タイムライン編集, インスペクタ, i18n) | D&D でのクリップ配置・分割・移動・Undo/Redo が動作 |
| **M3** | 再生エンジン (WebCodecs デコード, WebGL2 合成, AudioWorklet 同期) | 1080p60 2 レイヤーの滑らかな同期プレビュー再生 |
| **M4** | 書き出し (WebCodecs VideoEncoder + mp4-muxer) | タイムラインから MP4 動画ファイルへの書き出し完了 |
| **M5** | 字幕エンジン + エフェクト | SRT 取り込み → アニメーション付き表示および動画焼き込み |
| **M6** | AI オーケストレータ + AI トラック | リモート API 経由での T2V / TTS 生成とタイムラインコミット |
| **M7** | JS プラグイン SDK | サンプルプラグイン (Glitch) がロードされ画面にエフェクト適用 |

---

## 14.5 旧プロジェクトの互換性方針

- **v3 JSON の完全読み込み保証**:
  `SchemaMigration.migrate_3_to_4` により、旧バージョンで作成された `*.yave` ファイルは警告なしにそのままオープンできる。
- **未知フィールドの保全**:
  将来拡張されたフィールドや旧バージョンのレガシー設定は、`unknownFields` に保持され、再保存時にそのまま書き戻される。
