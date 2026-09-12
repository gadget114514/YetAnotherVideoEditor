# YAVE (Yet Another Video Editor)

C++17 / Qt 6 / FFmpeg ベースのクロスプラットフォーム動画編集アプリケーション。
詳細設計は `doc/design.md` および `doc/design/` 配下を参照。

## モジュール構成

| ターゲット | 内容 |
|---|---|
| `yave_core` | Rational / TimeRange / Clip / Track / Timeline / Project / Undo コマンド |
| `yave_subtitle` | 字幕クリップ / スタイル / SRT・VTT 入出力 / 組み込みエフェクト |
| `yave_io` | プロジェクト JSON シリアライズ (schemaVersion 管理あり) |
| `yave_audio` | AudioClock / ロックフリーリングバッファ / PDC / WASAPI デバイス |
| `yave_ai` | 生成パラメータ / タスク / オーケストレータ / プロバイダ |
| `yave_media` | FFmpeg デコード / エンコード / フレームキャッシュ (オプション) |
| `yave_render` | QRhi 合成パイプライン / テクスチャプール / シェーダ |
| `yave_plugin` | 字幕エフェクトレジストリ / VST3 (オプション) / AviUtl (Windows のみ) |
| `yave_app` | QML UI とエントリポイント |

## ビルド (Windows, Qt MinGW キット同梱環境)

```bash
cmake --preset win-mingw
cmake --build --preset win-mingw
```

Qt を別の場所にインストールしている場合は `CMakePresets.json` の
`CMAKE_PREFIX_PATH` を調整するか、直接指定する:

```bash
cmake -B build -G Ninja \
  -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 \
  -DCMAKE_C_COMPILER=C:/Qt/Tools/mingw1310_64/bin/gcc.exe \
  -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
cmake --build build
```

### FFmpeg

FFmpeg 開発ライブラリ (ヘッダ + インポートライブラリ) がある場合は
`-DFFMPEG_ROOT=<prefix>` を渡すと `yave_media` が実装込みでビルドされる。
無い場合は自動的にスタブモードになり、デコード/エンコードが「未対応」と
報告するだけで、他の機能とテストはすべて動作する。

macOS: `brew install ffmpeg`
Windows: vcpkg (`vcpkg.json` 同梱) または gyan.dev の dev パッケージ

### オプション一覧

| オプション | 既定 | 説明 |
|---|---|---|
| `YAVE_ENABLE_FFMPEG` | ON | 見つからなければ自動 OFF |
| `YAVE_ENABLE_AVIUTL` | Win: SDK 同梱時 ON | AviUtl x64 ホスト |
| `YAVE_ENABLE_VST3` | OFF | VST3 SDK を FetchContent |
| `YAVE_ENABLE_ONNX_LOCAL` | OFF | ローカル ONNX 推論 |
| `YAVE_BUILD_TESTS` | ON | Qt Test 単体テスト |

## テスト

```bash
ctest --preset win-mingw --output-on-failure
```

- `tst_rational`: 有理数演算・比較の境界値 (60000/1001 系タイムベース)
- `tst_timeline`: トラック不変条件・分割・リップル削除・Undo/Redo
- `tst_srtparser`: SRT パース (UTF-8 / Shift_JIS フォールバック) と書き出し往復
- `tst_projectserializer`: JSON 往復・未知フィールド保持・原子的保存
- `tst_delaycompensator`: PDC 計算と DelayLine

## ライセンス

TBD
