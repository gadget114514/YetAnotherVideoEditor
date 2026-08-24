# 3. タイムライン & レンダリングエンジン

[← 目次に戻る](../design.md)

---

## 3.1 時間表現

### 3.1.1 Rational

すべての時刻は有理数タイムベース上のフレーム番号で表す。

```cpp
// src/core/Rational.h
namespace yave {

struct Rational
{
    int64_t num = 0;
    int64_t den = 1;

    constexpr Rational() = default;
    constexpr Rational(int64_t n, int64_t d) : num(n), den(d) {}

    static constexpr Rational reduced(int64_t n, int64_t d);   // 約分して生成
    constexpr double toDouble() const { return den ? double(num) / double(den) : 0.0; }

    constexpr Rational inverted() const { return {den, num}; }

    Rational operator*(const Rational& o) const;
    Rational operator+(const Rational& o) const;
    bool operator==(const Rational& o) const;   // 約分して比較
    bool operator<(const Rational& o) const;    // 交差乗算。オーバーフローは __int128 / 分割で回避
};

// よく使うタイムベース
namespace timebase {
inline constexpr Rational Fps23_976{1001, 24000};
inline constexpr Rational Fps24    {1,    24};
inline constexpr Rational Fps25    {1,    25};
inline constexpr Rational Fps29_97 {1001, 30000};
inline constexpr Rational Fps30    {1,    30};
inline constexpr Rational Fps59_94 {1001, 60000};   // 既定
inline constexpr Rational Fps60    {1,    60};
}

/// 秒 -> フレーム番号。丸めモードを明示的に選ばせる(暗黙の切り捨てを禁止)
enum class RoundMode { Floor, Nearest, Ceil };
int64_t secondsToFrames(double seconds, const Rational& tb, RoundMode mode);
double  framesToSeconds(int64_t frames, const Rational& tb);

/// あるタイムベースのフレーム番号を別のタイムベースへ変換する
int64_t rescaleFrames(int64_t frames, const Rational& from, const Rational& to, RoundMode mode);

} // namespace yave
```

> **`operator<` でオーバーフローに注意**: `num` が `int64_t` のため交差乗算は容易に溢れる。
> 実装では `__int128` (GCC/Clang) / `_mul128` (MSVC) を使うか、`std::gcd` で約分してから比較する。
> テスト `tst_rational.cpp` に境界値ケースを必ず置くこと。

### 3.1.2 TimeRange

```cpp
// src/core/TimeRange.h
namespace yave {

/// 半開区間 [start, start + duration) をフレーム単位で表す。
/// end() は「含まれない最初のフレーム」。
struct TimeRange
{
    int64_t start    = 0;
    int64_t duration = 0;

    constexpr int64_t end() const { return start + duration; }
    constexpr bool isEmpty() const { return duration <= 0; }
    constexpr bool contains(int64_t f) const { return f >= start && f < end(); }
    constexpr bool intersects(const TimeRange& o) const
    { return start < o.end() && o.start < end(); }

    TimeRange intersected(const TimeRange& o) const;
    TimeRange translated(int64_t delta) const { return {start + delta, duration}; }
};

} // namespace yave
```

> **半開区間で統一する理由**: 閉区間だと隣接クリップの境界が 1 フレーム重なり、
> 「連結したはずのクリップが 1 フレーム被って点滅する」という不具合を生む。
> UI 表示上の Out 点は `end() - 1` として表示する(ユーザーは最終フレーム番号を見たがるため)。

## 3.2 無限レイヤー構造

### 3.2.1 データ構造の選択

```cpp
class Timeline
{
    std::vector<std::unique_ptr<Track>> tracks_;   // 無限レイヤー
};

class Track
{
    std::vector<std::shared_ptr<Clip>> clips_;     // start 昇順にソート済み(不変条件)
};
```

| 選択 | 理由 |
|---|---|
| `std::vector<std::unique_ptr<Track>>` | トラック数に上限を設けない。`std::rotate` による並び替えが O(n) で済む。トラック実体のアドレスが移動しないので、他所から `Track*` で参照しても安全 |
| Z オーダー = vector のインデックス | 別途 `zOrder` メンバを持つとソートと同期が必要になり、並び替えのバグ源になる。index 0 が最背面、末尾が最前面 |
| `std::shared_ptr<Clip>` | Undo コマンドがクリップを保持したまま Timeline から外す(削除の Undo)ため、所有権の共有が必要 |
| `clips_` は常に start 昇順 | 二分探索を可能にする。挿入時に順序を保つ責務は `Track::insertClip()` が持つ |

> **なぜ `std::list` や `std::map<int64_t, Clip>` でないか**: クリップ検索は再生中に毎フレーム
> 発生する(全トラック分)。連続メモリ上の二分探索が最も速い。挿入削除は編集操作時のみで、
> 頻度が桁違いに低いため O(n) の移動コストは問題にならない。

### 3.2.2 トラックの不変条件

`Track` は以下を常に満たす。デバッグビルドでは `assertInvariants()` を各操作後に呼ぶ。

1. `clips_` は `start` の昇順にソートされている。
2. 同一トラック内のクリップの `TimeRange` は互いに重ならない。
   - 重ねたい場合はトラックを分ける。これが「無限レイヤー」を前提にできる設計上の強み。
3. すべてのクリップの `duration > 0`。

### 3.2.3 クリップ検索

```cpp
// Track.cpp
std::shared_ptr<Clip> Track::clipAt(int64_t frame) const
{
    // start > frame となる最初の要素を探し、その 1 つ前が候補
    auto it = std::upper_bound(clips_.begin(), clips_.end(), frame,
        [](int64_t f, const std::shared_ptr<Clip>& c) { return f < c->range().start; });
    if (it == clips_.begin())
        return nullptr;
    --it;
    return (*it)->range().contains(frame) ? *it : nullptr;
}

std::vector<std::shared_ptr<Clip>> Track::clipsIn(const TimeRange& r) const
{
    std::vector<std::shared_ptr<Clip>> out;
    auto it = std::upper_bound(clips_.begin(), clips_.end(), r.start,
        [](int64_t f, const std::shared_ptr<Clip>& c) { return f < c->range().start; });
    if (it != clips_.begin()) --it;                 // 1 つ前が範囲に掛かる可能性
    for (; it != clips_.end() && (*it)->range().start < r.end(); ++it) {
        if ((*it)->range().intersects(r))
            out.push_back(*it);
    }
    return out;
}
```

計算量: `clipAt` は O(log n)、`clipsIn` は O(log n + k)。

### 3.2.4 トラックの種別と互換性

```cpp
enum class TrackType { Video, Audio, Subtitle, AiGenerated };
```

| TrackType | 受け入れる Clip | 合成への参加 |
|---|---|---|
| `Video` | `VideoClip` / `AiPlaceholderClip`(video系) | 映像レイヤーとして合成 |
| `Audio` | `AudioClip` / `AiPlaceholderClip`(audio系) | オーディオグラフへ |
| `Subtitle` | `SubtitleClip` | 映像レイヤーとして合成 (最前面寄りに置くのが慣例だが強制しない) |
| `AiGenerated` | 上記すべて | 生成結果の種別に応じて映像 or 音声 |

`AiGenerated` トラックは「生成物であることを UI 上で区別する」ためのマーカーであり、
振る舞いは中身のクリップ型で決まる。生成完了後にユーザーが普通の Video トラックへ
ドラッグ移動することもできる。

> **`Track::acceptsClip(const Clip&)` で受け入れ可否を判定**し、UI のドラッグ&ドロップは
> これを見てドロップ可否のフィードバックを出す。

### 3.2.5 トラック並び替え

```cpp
void Timeline::moveTrack(int from, int to)
{
    if (from == to || from < 0 || to < 0) return;
    if (from >= int(tracks_.size()) || to >= int(tracks_.size())) return;

    if (from < to)
        std::rotate(tracks_.begin() + from, tracks_.begin() + from + 1, tracks_.begin() + to + 1);
    else
        std::rotate(tracks_.begin() + to, tracks_.begin() + from, tracks_.begin() + from + 1);

    ++revision_;   // RenderSnapshot 再構築のトリガ
}
```

`ReorderTrackCommand` が `moveTrack(to, from)` を Undo として呼ぶ(引数を入れ替えるだけでは
`std::rotate` の性質上戻らないケースがあるため、実装ではコマンド側で元 index を保持し復元する)。

## 3.3 RenderSnapshot

Render Thread は `Timeline` を直接触らない。UI スレッドが 1 フレームごとに
不変のスナップショットを作り、Render Thread へ渡す。

```cpp
// src/core/RenderSnapshot.h
namespace yave {

struct LayerItem
{
    QUuid            clipId;
    TrackType        trackType   = TrackType::Video;
    int              zIndex      = 0;          // 小さいほど背面
    BlendMode        blendMode   = BlendMode::Normal;
    float            opacity     = 1.0f;
    QMatrix4x4       transform;                // レイヤー変換 (位置/拡縮/回転)
    QRectF           cropRect;                 // 0..1 正規化

    // 種別ごとの解決済み情報 (Render Thread がここから直接要求を出せるようにする)
    std::variant<std::monostate,
                 VideoSourceRef,               // assetId + sourceFrameIndex
                 SubtitleRenderRef,            // subtitle clip id + progress
                 GeneratedSourceRef>  source;
};

struct RenderSnapshot
{
    int64_t                 frameIndex = 0;
    Rational                timebase;
    QSize                   canvasSize;         // 出力解像度 (3840x2160 等)
    uint64_t                timelineRevision = 0;
    std::vector<LayerItem>  layers;             // 背面 -> 前面 の順に格納済み
};

} // namespace yave
```

構築は `Timeline::buildSnapshot(int64_t frame)` が行う。

```cpp
RenderSnapshot Timeline::buildSnapshot(int64_t frame) const
{
    RenderSnapshot snap;
    snap.frameIndex        = frame;
    snap.timebase          = timebase_;
    snap.canvasSize        = canvasSize_;
    snap.timelineRevision  = revision_;
    snap.layers.reserve(tracks_.size());

    int z = 0;
    for (const auto& track : tracks_) {          // index 0 = 最背面
        if (!track->isVisible() || track->type() == TrackType::Audio) { ++z; continue; }
        auto clip = track->clipAt(frame);
        if (!clip) { ++z; continue; }
        snap.layers.push_back(clip->makeLayerItem(frame, z, *track));
        ++z;
    }
    return snap;
}
```

コスト見積り: トラック 20 本で 20 回の二分探索 + 数百バイトのコピー。1 フレームあたり数マイクロ秒。

## 3.4 合成パイプライン

### 3.4.1 1 フレームの流れ

```
 [UI Thread]
   PlaybackController が AudioClock からフレーム番号 N を得る
        |
   Timeline::buildSnapshot(N)
        |
        v  (値渡し / move)
 [Render Thread]
   RhiCompositor::renderFrame(snapshot)
        |
   (1) PREPARE
        各 LayerItem のソースを要求
          - VideoSourceRef   -> FrameCache から QRhiTexture を取得
                                無ければ DecodeWorkerPool へ要求を出し、
                                今フレームは「直近の利用可能フレーム」で代用
          - SubtitleRenderRef-> SubtitleRenderer にグリフ変換配列を作らせる
          - GeneratedSourceRef-> 生成済みアセットのデコーダ(VideoSourceRef と同じ扱い)
        |
   (2) BEGIN FRAME
        rhi->beginFrame(swapChain)
        cb = swapChain->currentFrameCommandBuffer()
        |
   (3) COMPOSITE  (offscreen RT へ)
        compositeRt を canvasSize でクリア (透明黒)
        for layer in snapshot.layers:            // 背面 -> 前面
            LayerPass::draw(cb, layer, compositeRt)
              - 頂点: フルスクリーン三角形 (2 頂点キャッシュ)
              - 変換とブレンドモードは uniform で渡す
              - YUV テクスチャならフラグメントで RGB 変換
        |
   (4) PRESENT
        compositeRt のテクスチャをスワップチェーンへスケール描画
        (プレビューウィンドウのサイズに合わせる。アスペクト維持)
        rhi->endFrame(swapChain)
        |
   (5) STATS
        GPU タイムスタンプを回収し、フレーム時間を PerfMonitor へ
```

### 3.4.2 なぜオフスクリーン RT を挟むか

- 出力解像度 (4K) とプレビューウィンドウ解像度 (例 1280x720) を分離できる。
  **字幕は必ず出力解像度でラスタライズする**必要があるため、合成も出力解像度で行い、
  最後に一度だけ縮小する。これで「プレビューでは綺麗なのに書き出すと字幕が崩れる」を防ぐ。
- 書き出し (`ExportJob`) が同じ合成コードを再利用できる。書き出し時は (4) の代わりに
  `readback` してエンコーダへ渡すだけになる。

### 3.4.3 ブレンドモード

```cpp
// src/render/BlendMode.h
enum class BlendMode : int
{
    Normal = 0, Add, Multiply, Screen, Overlay, Darken, Lighten,
    ColorDodge, ColorBurn, Difference, Exclusion, AlphaMask
};
```

単一のフラグメントシェーダに uniform 分岐を持たせる。

```glsl
// src/render/shaders/layer_blend.frag
#version 440

layout(location = 0) in  vec2 vUv;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Ubuf {
    mat4  transform;
    vec4  cropRect;        // x, y, w, h  (0..1)
    float opacity;
    int   blendMode;
    int   colorSpace;      // 0=RGB, 1=BT709 limited, 2=BT709 full, 3=BT2020
    int   _pad;
} ub;

layout(binding = 1) uniform sampler2D srcTex;      // 上に乗せるレイヤー
layout(binding = 2) uniform sampler2D dstTex;      // ここまでの合成結果

vec3 blend(int mode, vec3 base, vec3 src)
{
    if (mode == 0)  return src;                                    // Normal
    if (mode == 1)  return base + src;                             // Add
    if (mode == 2)  return base * src;                             // Multiply
    if (mode == 3)  return 1.0 - (1.0 - base) * (1.0 - src);       // Screen
    if (mode == 4)  return mix(2.0 * base * src,
                               1.0 - 2.0 * (1.0 - base) * (1.0 - src),
                               step(0.5, base));                   // Overlay
    if (mode == 5)  return min(base, src);                         // Darken
    if (mode == 6)  return max(base, src);                         // Lighten
    if (mode == 9)  return abs(base - src);                        // Difference
    if (mode == 10) return base + src - 2.0 * base * src;          // Exclusion
    return src;
}

void main()
{
    vec2 uv = ub.cropRect.xy + vUv * ub.cropRect.zw;
    vec4 s  = texture(srcTex, uv);
    vec4 d  = texture(dstTex, vUv);

    s.a *= ub.opacity;

    if (ub.blendMode == 11) {                    // AlphaMask: src の輝度を dst のαに適用
        float lum = dot(s.rgb, vec3(0.2126, 0.7152, 0.0722));
        fragColor = vec4(d.rgb, d.a * lum);
        return;
    }

    vec3 blended = blend(ub.blendMode, d.rgb, s.rgb);
    fragColor    = vec4(mix(d.rgb, blended, s.a), max(d.a, s.a));
}
```

> **`dstTex` を読むために ping-pong RT が必要**: 描画先を同時にサンプリングすることは
> できないため、`compositeRtA` / `compositeRtB` の 2 枚を交互に使う。
> `Normal` ブレードのみのレイヤーが連続する場合は、GPU の固定機能ブレンド
> (`QRhiGraphicsPipeline::SrcAlpha` / `OneMinusSrcAlpha`) に切り替えて ping-pong を省略する
> 最適化を入れる。これが実運用では大半のケースになる。

### 3.4.4 シェーダのビルド

`.vert` / `.frag` は `qsb` (Qt Shader Baker) で事前コンパイルし、`.qsb` を `qrc` に埋め込む。

```cmake
# cmake/YaveShaders.cmake
function(yave_add_shaders target)
    qt6_add_shaders(${target} "${target}_shaders"
        PREFIX "/shaders"
        FILES ${ARGN}
        # Windows: HLSL(D3D11/12) + SPIR-V, macOS: MSL(Metal)
    )
endfunction()
```

```cmake
# src/render/CMakeLists.txt
yave_add_shaders(yave_render
    shaders/fullscreen.vert
    shaders/layer_blend.frag
    shaders/yuv_to_rgb.frag
    shaders/subtitle_glyph.vert
    shaders/subtitle_glyph.frag
)
```

実行時は `QShader::fromSerialized(QFile(":/shaders/layer_blend.frag.qsb").readAll())` で読む。
バックエンドごとの選択は `QRhi` が自動で行う。

## 3.5 テクスチャプール

4K RGBA8 テクスチャは 32MB。毎フレーム生成/破棄すると GPU メモリの断片化とスタッタが起きる。

```cpp
// src/render/TexturePool.h
class TexturePool
{
public:
    struct Key {
        QSize size;
        QRhiTexture::Format format;
        QRhiTexture::Flags flags;
        bool operator==(const Key&) const;
    };

    explicit TexturePool(QRhi* rhi, qint64 budgetBytes = 2LL * 1024 * 1024 * 1024);

    /// 取得。使い終わったら release() で返す。返却されたものは再利用される。
    QRhiTexture* acquire(const Key& key);
    void         release(QRhiTexture* tex);

    /// 一定フレーム数使われなかったテクスチャを解放する。フレーム末尾で呼ぶ。
    void         trim(int unusedFrameThreshold = 120);

    qint64 usedBytes() const;

private:
    QRhi* rhi_ = nullptr;
    qint64 budgetBytes_;
    struct Entry { std::unique_ptr<QRhiTexture> tex; int lastUsedFrame; bool inUse; };
    std::unordered_multimap<Key, Entry, KeyHash> pool_;
    int currentFrame_ = 0;
};
```

予算超過時は LRU で解放する。それでも足りない場合は `acquire` が `nullptr` を返し、
`RhiCompositor` はそのレイヤーの描画をスキップして警告ログを出す(クラッシュさせない)。

## 3.6 4K60p のフレーム予算

1 フレーム = 16.67ms。目標配分:

| 工程 | 予算 | 備考 |
|---|---|---|
| `buildSnapshot` (UI) | 0.1ms | 二分探索のみ |
| デコード待ち | 0ms | **先読みで既に完了している前提**。同期待ちが発生したら設計上の失敗 |
| テクスチャアップロード | 1.0ms | ゼロコピー成功時は 0ms。フォールバック時のみ |
| 字幕グリフ変換計算 | 0.5ms | エフェクトスタック適用。CPU |
| レイヤー合成 (4 層) | 6.0ms | GPU。1 層あたり 1.5ms (4K フルスクリーン矩形 4 パス) |
| 字幕描画 | 1.0ms | インスタンシング 1 ドローコール |
| プレゼント / スケール | 1.0ms | |
| 余裕 | 7.0ms | VST3 処理や OS のジッタ吸収 |

### 3.6.1 予算超過時の劣化戦略 (Adaptive Quality)

超過を検知したら以下の順に品質を落とす。落とした事実は UI にインジケータで明示する
(黙って劣化させるとユーザーは「書き出したら違う」と混乱するため)。

```
Level 0 (通常)      : 出力解像度で合成
Level 1             : プレビューのみ 1/2 解像度で合成 (字幕は出力解像度のまま)
Level 2             : プレビューのみ 1/4 解像度 + 字幕も 1/2 解像度
Level 3             : フレームスキップ (2 フレームに 1 回描画)
Level 4             : 最前面 N レイヤーのみ描画し、それ以外は静止画で代用
```

判定は直近 30 フレームの中央値フレーム時間で行う (平均だと単発スパイクに過剰反応するため)。
`> 15ms` で 1 段下げ、`< 9ms` が 60 フレーム続いたら 1 段上げる (ヒステリシス)。

### 3.6.2 ドロップフレーム方針

再生中に指定フレームのデコードが間に合わない場合:

- **音声は絶対に途切れさせない**。オーディオがマスタークロックなので、
  音を止めるとタイムライン全体が止まる。
- 映像は「直近に取得できた最新フレーム」を表示し続ける (フレームホールド)。
- ドロップ数は `PerfMonitor` に記録し、ステータスバーに表示する。

## 3.7 プレビュー表示 (QML への統合)

`QQuickRhiItem` (Qt 6.7+) を継承した `PreviewItem` を使う。
Qt 6.6 環境では `QQuickFramebufferObject` 相当の自前実装が必要になるため、
**Qt 6.7 以上を要求する**ことを推奨する。

```cpp
// src/app/items/PreviewItem.h
class PreviewRenderer : public QQuickRhiItemRenderer
{
public:
    void initialize(QRhiCommandBuffer* cb) override;
    void synchronize(QQuickRhiItem* item) override;   // UI スレッドで呼ばれる
    void render(QRhiCommandBuffer* cb) override;      // Render スレッドで呼ばれる

private:
    RenderSnapshot pendingSnapshot_;   // synchronize でコピーされる
    std::unique_ptr<RhiCompositor> compositor_;
};

class PreviewItem : public QQuickRhiItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qint64 frameIndex READ frameIndex WRITE setFrameIndex NOTIFY frameIndexChanged)
public:
    QQuickRhiItemRenderer* createRenderer() override;
    void setTimeline(Timeline* tl);
    // ...
};
```

`synchronize()` は UI スレッドと Render スレッドがブロックし合う唯一のタイミングであり、
ここで `RenderSnapshot` をコピーする。それ以外では Timeline に触れない。

## 3.8 編集操作の実装 (代表例)

すべて `QUndoCommand` 派生。ここでは代表的な 3 つの挙動を定義する。

### 3.8.1 クリップ分割 (Split)

```cpp
void SplitClipCommand::redo()
{
    auto clip = track_->takeClip(clipId_);
    original_ = clip;                             // Undo 用に保持

    const TimeRange r = clip->range();
    const int64_t   offset = splitFrame_ - r.start;

    left_  = clip->clone();
    right_ = clip->clone();
    left_->setRange({r.start, offset});
    right_->setRange({splitFrame_, r.duration - offset});
    right_->shiftSourceOffset(offset);            // ソース内オフセットも進める

    // 字幕なら: テキストは両方に複製、エフェクトスタックも複製。
    // タイプライター等の progress 依存エフェクトは区間が変わるので見た目が変わるが、
    // これは仕様として許容し、UI で「分割するとアニメーションが再計算されます」と注記する。

    track_->insertClip(left_);
    track_->insertClip(right_);
}
```

### 3.8.2 リップル削除

削除したクリップより後ろのクリップを、そのトラックだけ前に詰める。
全トラック連動の「リップル編集」モードでは全トラックで同じ処理を行う。

```cpp
void RippleDeleteCommand::redo()
{
    const TimeRange gone = removedRange_;
    for (Track* t : affectedTracks_) {
        t->removeClipsIn(gone);
        t->shiftClipsAfter(gone.start, -gone.duration);
    }
}
```

### 3.8.3 スナップ

ドラッグ中の吸着候補:

1. 再生ヘッド
2. 同一トラック内の隣接クリップ端
3. **他トラックのクリップ端** (レイヤーが多いと候補が爆発するので、
   画面に見えているトラックのみを対象にする)
4. マーカー
5. 秒単位のグリッド

候補は `std::vector<int64_t>` に集めてソートし、`std::lower_bound` で最寄りを探す。
吸着閾値はピクセル単位 (既定 8px) をフレーム数に換算して使う
(ズームレベルによらず操作感を一定にするため)。
