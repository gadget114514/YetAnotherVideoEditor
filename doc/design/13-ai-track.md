# 13. AIトラック (演出指示 / 絵コンテトラック)

[← 目次に戻る](../design.md)

---

## 13.1 このトラックは何か

### 13.1.1 位置づけ

**AIトラック**は、タイムライン上に「**これから何を作るか**」を並べるトラックである。
映像や音声そのものは載らない。載るのは**カット単位の演出指示** (`CutClip`) であり、
これは絵コンテ (storyboard) の 1 コマに相当する。

生成を実行すると、各カットは自分が必要とする**出力トラック** (映像 / ナレーション /
BGM / SE / 字幕) を解決または新規作成し、そこへ実クリップを配置する。
AIトラックは生成後も残り続け、**再生成の source of truth** になる。

```
  +- Storyboard トラック (AIトラック) ------------------------------+
  | [カット1 屋上・朝] [カット2 教室] [カット3 廊下] [カット4 …]      |  <- 仕様。合成に参加しない
  +--------+---------------+--------------+------------------------+
           | 生成           |              |
           v               v              v
  +- Video (aiRole=mainVideo) --------------------------------------+
  | [   生成映像1   ] [   生成映像2   ] [   生成映像3   ]              |
  +----------------------------------------------------------------+
  +- Subtitle (aiRole=subtitle) ------------------------------------+
  | [ セリフ1 ]        [ セリフ2 ]       [ セリフ3 ]                   |
  +----------------------------------------------------------------+
  +- Audio (aiRole=narration) --------------------------------------+
  | [ TTS1     ]       [ TTS2     ]      [ TTS3     ]                |
  +----------------------------------------------------------------+
```

### 13.1.2 L1 と L2

AI へのリクエストは 2 階層ある。両方を本章で定義する。

| 階層 | 入力 | 出力 | 実行主体 |
|---|---|---|---|
| **L1 プランニング** | 自然言語の要求 (「3 分の商品紹介動画を作って」) | カット列 + 必要トラック構成 + Story Bible 草案 | LLM (`GenerationKind::Storyboard`) |
| **L2 マテリアライズ** | 1 カットの演出指示 | 映像 / 音声 / 字幕 の実メディア | [7 章](07-ai-orchestrator.md)の既存パイプライン |

L1 は**タイムラインへ直接メディアを置かない**。カットを置くだけである。
L2 は既存の `AiGenerationOrchestrator` をそのまま使い、本章はその上に
「1 カット → N タスク」の展開と「複数カット → 依存順バッチ」を積む。

### 13.1.3 なぜ `AiGenerated` を再利用しないのか

[3.2.4](03-timeline-render.md) は `TrackType::AiGenerated` について次のように定めている。

> `AiGenerated` トラックは「生成物であることを UI 上で区別する」ためのマーカーであり、
> 振る舞いは中身のクリップ型で決まる。

この記述は**正しく、変更しない**。AIトラックはこれとは逆の性質を持つ。

| | `AiGenerated` | `Storyboard` (本章) |
|---|---|---|
| 何が載るか | 生成された実クリップ (Video/Audio/Subtitle) | `CutClip` のみ |
| 合成への参加 | する (中身のクリップ型に従う) | **しない** |
| 振る舞い | 無し (マーカー) | 有り (出力解決・バッチ生成・アニマティック) |
| 生成後の扱い | 通常トラックへドラッグ移動可能 | 移動しない。仕様として残る |

同一の enum 値に 2 つの意味を持たせると `Track::acceptsClip()` と
`Timeline::buildSnapshot()` の両方が「どちらの AiGenerated か」を判別する必要が生じ、
マーカー規則が静かに壊れる。よって**別の enum 値を追加する**。

> **既存プロジェクトへの影響はない。** `AiGenerated` の意味論も JSON 表現も一切変わらない。

## 13.2 用語

| 用語 | 意味 |
|---|---|
| **カット** | AIトラック上の 1 区間 (`CutClip`)。絵コンテの 1 コマ。演出指示の単位 |
| **絵コンテトラック / AIトラック** | 本書では同義。`TrackType::Storyboard` のトラック |
| **出力バインディング** | 1 カットが生む 1 つの成果物の仕様 (`OutputBinding`)。役割・配置先・パラメータ・生成状態を持つ |
| **役割 (role)** | 出力の種別 (`OutputRole`)。mainVideo / narration / bgm / se / subtitle / mask など |
| **Story Bible** | プロジェクト単位の設定集。キャラクター・ロケーション・画風・ネガティブ。全カットのプロンプトへカスケードする |
| **アニマティック** | 未生成カットをボード画像 + TTS プレビューで仮再生すること。絵コンテ動画 |
| **承認状態** | カットの進行状態 (`CutStatus`)。未着手 / ラフ / 確認中 / 承認済み |
| **仕様ハッシュ** | 出力バインディング 1 件の生成仕様を要約した SHA-256。再生成要否の判定に使う |
| **プラン** | L1 が生成する JSON。カット列とトラック構成の設計案 |

## 13.3 データモデル

### 13.3.1 TrackType の拡張

```cpp
// src/core/Track.h
enum class TrackType { Video, Audio, Subtitle, AiGenerated, Storyboard, Unknown };
```

`Storyboard` (JSON `"storyboard"`) を追加する。あわせて `Unknown` を追加する
(理由は 13.10.4)。

`Track` に以下を追加する。

```cpp
class Track
{
public:
    /// 映像合成に参加するか。Video / Subtitle / AiGenerated(映像系) が true。
    /// Storyboard / Audio / Unknown は false。
    bool participatesInComposite() const;

    /// オーディオグラフに参加するか。
    bool participatesInAudioGraph() const;

    // --- AIトラックとの関係 ---
    /// このトラックが担う出力役割。空文字なら手動作成の通常トラック。
    /// "mainVideo" / "mainVideoB" / "overlay" / "narration" / "bgm" / "se" / "subtitle" / "mask"
    QString aiRole() const;
    void    setAiRole(const QString& r);

    /// このトラックを生成した絵コンテトラックの ID。null なら無関係。
    QUuid   storyboardTrackId() const;
    void    setStoryboardTrackId(const QUuid& id);

    /// 役割ごとの既定パラメータ (カスケード第 2 段)。
    const QJsonObject& roleDefaultsPatch() const;

    /// 新規: 未知フィールドの保持 (Clip と同じ前方互換の仕組み)
    const QJsonObject& unknownFields() const;
    void setUnknownFields(const QJsonObject& o);
};
```

`acceptsClip()` の規則:

| TrackType | 受け入れる Clip |
|---|---|
| `Storyboard` | **`CutClip` のみ** |
| その他 | `CutClip` を**受け入れない** |

これは全トラック型の中で唯一「排他的」な受け入れ規則である。理由:
カットはメディアではなく**仕様**であり、同じトラックにメディアが混ざると
「AIトラックが真実である」という前提が成り立たなくなる。

> **絵コンテトラックは複数あってよい。** A/B 案の比較、シーン単位のボード分割などに使う。
> 出力の解決は `storyboardTrackId` でスコープされるため、
> 別の絵コンテトラックが同じ出力トラックを取り合うことはない。

### 13.3.2 CutClip

```cpp
// src/core/Clip.h
enum class ClipType { Video, Audio, Subtitle, AiPlaceholder, Image, Color, Cut };
```

> **命名について**: 領域の用語は「カット」なので `CutClip` とする。
> クリップボードの "cut" と紛らわしいため、**コーディング規約として
> `EditController` のクリップボード操作は `copySelection` / `removeSelection` / `paste`
> と命名し、`cut` という名前のメソッドを作らない**。

```cpp
// src/core/CutClip.h
namespace yave {

enum class CutStatus      { NotStarted, Rough, InReview, Approved };
enum class ContinuityMode { None, FromBoardImage, FromPreviousEnd, FromCutId };

enum class ShotSize    { Unspecified, ExtremeWide, Wide, Full, Medium, CloseUp, ExtremeCloseUp };
enum class CameraAngle { Unspecified, EyeLevel, High, Low, BirdsEye, WormsEye, Dutch };
enum class CameraMovement { Unspecified, Fixed, PanLeft, PanRight, TiltUp, TiltDown,
                            Dolly, SlowPushIn, PullOut, Handheld, Crane, Follow };
enum class TransitionKind { Cut, Dissolve, FadeToBlack, FadeFromBlack, Wipe, MatchCut };

struct CameraWork
{
    ShotSize       size     = ShotSize::Unspecified;
    CameraAngle    angle    = CameraAngle::Unspecified;
    CameraMovement movement = CameraMovement::Unspecified;
    QString        note;                 // 自由記述 ("手前の柵越しに")
};

/// 絵コンテの「画」。
struct BoardImage
{
    enum class Origin { None, UserFile, Generated, TimelineFrame };
    Origin  origin = Origin::None;
    QUuid   assetId;            // UserFile / Generated: AssetLibrary 登録済み
    QUuid   sourceTrackId;      // TimelineFrame
    int64_t sourceFrame = 0;    // 同上
    QUuid   generatedByTaskId;  // Generated (T2I) の由来タスク
};

/// 前後カットとの繋がり方。
struct Continuity
{
    ContinuityMode mode       = ContinuityMode::FromBoardImage;
    QUuid          fromCutId;             // FromCutId のときのみ
    double         strength   = 0.9;      // ImageReference::strength へ写す
    bool           sceneBreak = false;    // true でチェーンを切る (並列化の単位)
};

/// AIトラック上の 1 区間 = 1 カットの演出指示。
class CutClip : public Clip
{
public:
    ClipType type() const override { return ClipType::Cut; }
    std::shared_ptr<Clip> clone() const override;

    // ---- 人間向けの仕様。これが真実である ----
    QString slug()        const;   // 見出し「屋上・朝」
    QString label()       const;   // 手動採番 "12b"。既定は空 (自動採番を使う)
    QString description() const;   // ト書き。何が起きるか
    QString dialogue()    const;   // セリフ / ナレーション原稿
    QString mood()        const;   // 「静か / 寂しい」

    CameraWork camera() const;
    const std::vector<QUuid>& characterIds() const;   // StoryBible への参照
    QUuid   locationId() const;

    TransitionKind transitionIn()  const;
    TransitionKind transitionOut() const;

    BoardImage board() const;

    // ---- 連続性 / 承認 ----
    Continuity continuity() const;
    CutStatus  status()     const;
    QString    reviewNote() const;

    // ---- 生成 ----
    const std::vector<OutputBinding>& outputs() const;
    OutputBinding*       findOutput(const QUuid& bindingId);
    const OutputBinding* findOutput(OutputRole role, const QString& tag = {}) const;

    /// カット段のカスケード層 (疎パッチ)。13.3.5 参照
    const QJsonObject& paramPatch() const;
    /// artStyle / negativePrompt などをこのカットだけ上書きする
    const QJsonObject& biblePatch() const;

    // ---- 表示 ----
    /// アニマティック専用。合成には参加しない。13.8 参照
    LayerItem makeLayerItem(int64_t frame, int zIndex, const Track& t) const override;

    /// 未生成カットのキャプション表示に使う一時 SubtitleClip。所有はこのクリップ。
    const subtitle::SubtitleClip* animaticCaption() const;

    /// 指定役割の生成仕様ハッシュ。13.6.2 参照。
    /// status / reviewNote / label / slug / resolvedTrackId / committed* / state は含めない。
    QByteArray specHash(OutputRole role, const QString& tag = {}) const;
};

} // namespace yave
```

#### カット番号を保持しない

カット番号は `Track::clips_` 内のインデックスから導出する。フィールドとして持たない。

> **理由**: [3.2.1](03-timeline-render.md) が `zOrder` フィールドを拒否したのと同じ論理である。
> 番号を実体として持つと、挿入・削除・並び替えのたびに再採番と同期が必要になり、
> ずれたときに「表示は 12 番だが内部は 11 番」という追跡困難なバグになる。
> 配列順序を唯一の真実にすれば、この破綻が構造的に起きない。

手動で「12b」のような番号を振りたい要求のために `label` を別に持つ。
`label` が空なら UI は導出番号を表示する。

### 13.3.3 OutputBinding — 「必要なトラックを生成する」の実体

1 カットは 0 個以上の `OutputBinding` を持ち、**1 バインディング = 1 生成タスク = 1 出力**
である。これが「AI にリクエストすると必要なトラックが生える」挙動の担い手になる。

```cpp
// src/core/CutClip.h
namespace yave {

enum class OutputRole {
    MainVideo,     // 主映像
    MainVideoB,    // トランジション用の B ロール (13.14.7)
    Overlay,       // 前景オーバーレイ
    Narration,     // ナレーション (TTS)
    Bgm,
    SoundEffect,
    Subtitle,
    Mask
};

enum class TrackResolveMode { Auto, Existing, AlwaysNew };

enum class OutputState {
    NotGenerated,  // 未生成
    Queued, Running, Cached,
    Committed,     // タイムラインに反映済み
    Failed,
    Stale,         // コミット済みだが仕様が変わった
    Blocked        // 上流カットが失敗したため生成しない
};

struct OutputBinding
{
    QUuid       id;                     // 安定 ID。Undo と再生成をまたいで維持する
    OutputRole  role    = OutputRole::MainVideo;
    QString     roleTag;                // 同一役割の複数出力を区別する "se.door"
    bool        enabled = true;

    // --- 配置先の解決 ---
    TrackResolveMode resolveMode = TrackResolveMode::Auto;
    QUuid       resolvedTrackId;        // 解決後に書き戻される
    QString     trackNameHint;

    // --- 出力間の依存 ---
    QUuid       derivedFromBindingId;   // 例: subtitle は narration の出力から STT

    // --- 尺 ---
    int64_t     leadInFrames  = 0;      // カット区間に対する前伸ばし
    int64_t     leadOutFrames = 0;      // トランジション用の後伸ばし

    // --- 仕様 (カスケード最終段) ---
    QJsonObject paramPatch;
    PromptLock  promptLock;             // 13.4.4

    // --- 結果 ---
    QUuid       lastTaskId;
    std::vector<QUuid> committedClipIds;   // 字幕は複数キューになりうる
    QByteArray  committedSpecHash;         // コミット時点の自分の仕様
    QByteArray  committedUpstreamHash;     // コミット時点の上流の仕様
    OutputState state = OutputState::NotGenerated;
};

} // namespace yave
```

#### 役割 → トラック型の対応

| OutputRole | TrackType | 生成される Clip |
|---|---|---|
| `MainVideo` / `MainVideoB` / `Overlay` | `Video` | `VideoClip` |
| `Mask` | `Video` (AlphaMask ブレンド) | `VideoClip` |
| `Narration` / `Bgm` / `SoundEffect` | `Audio` | `AudioClip` |
| `Subtitle` | `Subtitle` | `SubtitleClip` 群 |

#### 解決アルゴリズム (`RoleTrackResolver`)

```cpp
// src/ai/RoleTrackResolver.h
class RoleTrackResolver
{
public:
    struct Resolution {
        QUuid     trackId;
        bool      needsCreation = false;
        int       insertIndex   = -1;      // needsCreation のとき
        TrackType trackType     = TrackType::Video;
        QString   trackName;
    };

    /// 副作用なし。Undo コマンドが実際の生成を行う。
    Resolution resolve(const Timeline& tl,
                       const Track& storyboardTrack,
                       const CutClip& cut,
                       const OutputBinding& b) const;
};
```

判定順序:

1. `b.resolvedTrackId` が非 null かつトラックが実在し型が互換 → **それを使う**
2. `resolveMode == Existing` で 1 が満たされない → `error.ai.cut.boundTrackMissing`
3. `resolveMode == Auto` → `aiRole == roleKey(role, roleTag)` かつ
   `storyboardTrackId == storyboardTrack.id()` のトラックを検索。最初の一致を使う
4. 見つからない、または `AlwaysNew` → 新規作成

新規作成時の Z 配置は**役割の優先度表で決定的に**決める。

| 挿入位置 | 役割 |
|---|---|
| 絵コンテトラックの直下から順に | `Mask`, `MainVideo`, `MainVideoB`, `Overlay` |
| その上 | `Subtitle` |
| トラック列の末尾 | `Narration`, `SoundEffect`, `Bgm` (音声は Z 順に無関係) |

これにより、同じプランから 2 回作っても同じ Z 順になる。

> **冪等性が最重要**: `resolvedTrackId` の書き戻しは、トラックを作った
> `AddTrackCommand` を**子コマンドに持つ同一の Undo ステップ**内で行う (13.9)。
> 分けると、undo でトラックだけ消えて `resolvedTrackId` が宙を指す状態が到達可能になる。
> また判定 1 が最初に来ることで、バッチを 2 回流してもトラックは増殖しない。

### 13.3.4 StoryBible

```cpp
// src/core/StoryBible.h
namespace yave {

struct StoryBibleCharacter
{
    QUuid   id;                  // 参照用。design.md §3.3 に従い UUID
    QString key;                 // 人間可読キー "aoi"。プロンプト内参照とモデル出力の照合用
    QString name;                // 表示名「葵」
    QString appearance;          // 「黒髪ショート、紺のセーラー服」
    QString personality;
    QString promptFragment;      // モデルへ渡す英語断片
    QString voiceId;             // TTS の既定話者
    ai::ImageReference referenceImage;
};

struct StoryBibleLocation
{
    QUuid   id;
    QString key;                 // "rooftop"
    QString name;
    QString description;
    QString promptFragment;
    ai::ImageReference referenceImage;
};

/// プロジェクト単位の作品設定。Project が所有する。
struct StoryBible
{
    QString artStyle;            // 共通画風
    QString negativePrompt;      // 全カット共通のネガティブ
    QString promptPrefix;
    QString promptSuffix;

    std::vector<StoryBibleCharacter> characters;
    std::vector<StoryBibleLocation>  locations;

    QJsonObject roleDefaults;    // role -> 疎パッチ (カスケード第 1 段)
    QJsonObject promptTemplates; // role -> テンプレート文字列 (13.4)

    QJsonObject unknownFields;

    const StoryBibleCharacter* characterById(const QUuid&) const;
    const StoryBibleCharacter* characterByKey(const QString&) const;
    const StoryBibleLocation*  locationById(const QUuid&) const;
    const StoryBibleLocation*  locationByKey(const QString&) const;
};

} // namespace yave
```

> **`id` と `key` を両方持つ理由**: `design.md` §3.3 は「ID は UUID、配列インデックスを
> 永続 ID にしない」と定めており、カットからの参照は `QUuid id` で行う。
> 一方 L1 の LLM は UUID を発明できず、プロンプト内でも `{{characters}}` として
> 人間可読な名前で扱いたい。そこで**参照は UUID、照合と表示は `key`** に分ける。
> `key` は `[a-z0-9._-]{1,64}` で検証し、インポート時に一意化する。
> `key` を変えても既存カットの参照は壊れない。

### 13.3.5 カスケード — Story Bible → トラック → カット → バインディング

生成に使う `ai::AiGenerationParams` は、4 段のマージで組み立てる。

```
  役割ごとのベースライン (組み込み既定)
        | merge
  StoryBible::roleDefaults[role]
        | merge
  Track::roleDefaultsPatch()[role]        (絵コンテトラック単位の既定)
        | merge
  CutClip::paramPatch()                   (カット単位)
        | merge
  OutputBinding::paramPatch               (出力単位)
        v
  解決済み AiGenerationParams + provenance
```

```cpp
// src/ai/ParamCascade.h
class ParamCascade
{
public:
    struct Resolved {
        ai::AiGenerationParams params;
        QJsonObject provenance;   // キー -> 出所 ("bible.roleDefaults" / "cut" / "binding")
    };

    Resolved resolve(const StoryBible& bible,
                     const Track& storyboardTrack,
                     const CutClip& cut,
                     const OutputBinding& binding) const;
};
```

#### なぜ `std::optional` の列ではなく疎な JSON パッチなのか

`AiGenerationParams` は値初期化されたフラットな構造体である。
そこに値を直接置くと **「ユーザーが意図して `guidanceScale = 7.5` にした」と
「たまたま既定値が 7.5 だった」を区別できない**。
区別できなければ「継承に戻す」ボタンも「上書き中」インジケータも実装できない。

疎パッチなら:

- **キーの存在そのものが「上書き済み」の定義**になる
- 「継承に戻す」= `patch.remove(key)`
- 未知キーが素通りするので、`Clip::unknownFields_` と同じ理屈で前方互換になる

代償は文字列キーであること。緩和策を規約として定める。

1. キー名は必ず `io::keys` の定数を使い、リテラルを書かない
2. `tst_cutcascade.cpp` が「`AiGenerationParams::toJson()` が出す全キーは
   マージ経路を通過して往復する」ことを表明する

#### どの段が何を持つか

| フィールド群 | 既定の所在 | 上書き可能な段 |
|---|---|---|
| `artStyle` / `negativePrompt` / prefix / suffix | Story Bible | カット (`biblePatch`) |
| `modelId` / `providerId` / `steps` / `guidanceScale` / `outputResolution` / `outputFrameRate` | Bible `roleDefaults` | トラック → カット → バインディング |
| `voiceId` | キャラクター定義 → Bible `roleDefaults` | カット → バインディング |
| `speakingRate` / `pitch` / `targetLufs` | Bible `roleDefaults` | カット → バインディング |
| `seed` | 常に `-1` (**継承しない**) | バインディングのみ |
| `range` | **継承不可**。`cut.range()` と `leadIn/OutFrames` から必ず導出する | なし |
| `targetTrackId` / `createNewTrack` / `replaceExistingClips` | **継承不可**。`OutputBinding` が唯一の真実 | なし |

> `seed` を継承させない理由: 継承すると Story Bible に seed を書いた瞬間、
> 全カットが同じ seed で生成され、意図せず似た画になる。
> 再現性が要るのは「このカットをもう一度」であって「全カットを同じ乱数で」ではない。

## 13.4 プロンプト合成

### 13.4.1 CutPromptComposer

カットの構造化フィールドは**人間のための表現**であり、モデルへ渡す文字列とは別物である。
両者を分けたまま、決定的に片方からもう片方を作る。

```cpp
// src/ai/CutPromptComposer.h
namespace yave::ai {

struct ComposedPrompt
{
    QString     prompt;
    QString     negativePrompt;
    QJsonObject provenance;    // 断片 -> 出所 ("bible.artStyle" / "cut.description" / …)
    QByteArray  sourceHash;    // 合成元となった仕様のハッシュ
    QStringList warnings;      // 未解決プレースホルダ等
    bool        fromLock = false;
};

class CutPromptComposer
{
public:
    ComposedPrompt compose(const StoryBible& bible,
                           const CutClip& cut,
                           const OutputBinding& binding) const;

    /// 組み込み既定テンプレート (:/ai/prompt_templates.json)
    static QString defaultTemplate(OutputRole role);
};

} // namespace yave::ai
```

### 13.4.2 テンプレート

役割ごとのテンプレートは `StoryBible::promptTemplates` に**永続化する**。
プロジェクトごとに調整でき、差分が見え、共有できる。

```
mainVideo:  "{{artStyle}}. {{location}}. {{characters}}. {{description}} {{camera}} {{mood}}"
narration:  "{{dialogue}}"
subtitle:   "{{dialogue}}"
bgm:        "{{mood}}, {{artStyle}}, instrumental"
```

| プレースホルダ | 展開元 |
|---|---|
| `{{artStyle}}` | `bible.artStyle` (カットの `biblePatch` で上書き可) |
| `{{location}}` | `bible.locationById(cut.locationId())->promptFragment` |
| `{{characters}}` | `cut.characterIds()` を `promptFragment` へ写して連結 |
| `{{description}}` | `cut.description()` |
| `{{dialogue}}` | `cut.dialogue()` |
| `{{camera}}` | `cut.camera()` をモデル向けフレーズ表で展開 |
| `{{mood}}` | `cut.mood()` |
| `{{transition}}` | `cut.transitionIn/Out()` |

未知のプレースホルダは**空に展開し警告を積む**。テンプレートはユーザーが編集する
対象なので、例外を投げてはならない。

### 13.4.3 プロンプトは翻訳経路に載せない

`CameraMovement::SlowPushIn` → `"slow push-in"` のような**モデル向けフレーズ表は
UI 翻訳とは完全に別系統**にする。

> **規約**: プロンプト文字列を `tr()` / `qsTr()` に通してはならない。
> UI 言語を日本語にしただけでモデルへ渡る語彙が変わると、同じプロジェクトが
> 環境によって違う絵を出す。UI ラベルは `tr()`、プロンプト断片は
> `CameraPhraseTable` (英語固定、`:/ai/camera_phrases.json`) から取る。
> [10 章](10-i18n.md) にもこの規約を追記する。

### 13.4.4 手編集とロック

```cpp
struct PromptLock
{
    bool       locked = false;
    QString    prompt;
    QString    negativePrompt;
    QByteArray lockedAgainstHash;   // ロックした時点の ComposedPrompt::sourceHash
};
```

- カットインスペクタは合成後のプロンプトを**読み取り専用**で表示し、
  provenance ガター (どのフィールド由来かの帯) を添える。断片をクリックすると
  該当フィールドへジャンプする。
- 「編集 / ロック」を押すと合成結果が `PromptLock` にコピーされ、以後は
  ロック文字列が使われる。
- ロック後に仕様が変わると `compose().sourceHash != lockedAgainstHash` になり、
  「仕様が変更されています」バッジと「再合成」「差分を見る」を出す。
  **黙って再合成もしないし、黙って古いまま使うこともしない。**
- ロックは**カット単位ではなく `OutputBinding` 単位**。映像のプロンプトと
  ナレーション原稿は別物であり、片方をロックしても他方は追従してよい。
- `specHash` はロック中ならロック文字列を含む。よってロック・編集は正しく
  「再生成が必要」と判定される。

## 13.5 選択モデル

「選択した区間に対して生成する」には選択の定義が要るが、**既存 12 章のどこにも
選択モデルが定義されていない**。[6.9.2](06-subtitle-engine.md) の
`addEffectToSelectedSubtitles()` の時点で既に穴が空いている。本節で最小限を埋める。

```cpp
// src/app/models/SelectionModel.h
namespace yave::app {

enum class SelectionMode { Replace, Add, Toggle, ExtendRange };

struct TimelineSelection
{
    std::vector<QUuid>       clipIds;
    std::vector<QUuid>       trackIds;
    std::optional<TimeRange> range;
    std::vector<QUuid>       rangeTrackIds;   // 空 = 全トラックに掛かる範囲選択
    QUuid                    primaryClipId;   // インスペクタが表示する主対象
};

class SelectionModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int  clipCount READ clipCount NOTIFY selectionChanged)
    Q_PROPERTY(bool hasRange  READ hasRange  NOTIFY selectionChanged)
public:
    Q_INVOKABLE void selectClip(const QUuid& id, SelectionMode m);
    Q_INVOKABLE void selectTrack(const QUuid& id, SelectionMode m);
    Q_INVOKABLE void setRange(qint64 start, qint64 duration);
    Q_INVOKABLE void clear();

    const TimelineSelection& selection() const;

    /// 生成対象カットの解決:
    ///   明示選択されたカット ∪ (range に交差する可視 Storyboard トラック上のカット)
    std::vector<QUuid> resolvedCutIds(const Timeline& tl) const;

signals:
    void selectionChanged();
};

} // namespace yave::app
```

規則:

- **Undo の対象にしない。プロジェクト JSON にも保存しない。**
  永続するカーソル状態は既存の `playhead` と `workRange` だけである。
- ID のみを保持する。`Timeline::clipRemoved` / `trackRemoved` を購読して自己修復する。
- カット解決は「明示選択が優先。空なら範囲に**交差**するカット」。
  部分的にしか掛かっていないカットも含めるが、確認ダイアログにその旨を書く。
- `EditController` が所有し、QML へコンテキストプロパティとして公開する。
  `StoryboardController` は読むだけ。

> **ボードビューとタイムラインの双方向同期は、両者が同じ `SelectionModel` に
> bind することで自動的に満たされる。** 個別に同期コードを書かない。

## 13.6 バッチ生成

選択したカット群をまとめて生成する。`StoryboardBatchJob` が
`AiGenerationOrchestrator` の**上位**に立つ。オーケストレータは従来どおり
「1 タスク = 1 生成」のままで、バッチの概念を持ち込まない。

```cpp
// src/ai/StoryboardBatchJob.h
namespace yave::ai {

struct BatchNode
{
    QUuid            cutId;
    QUuid            bindingId;
    QByteArray       specHash;
    QByteArray       upstreamHash;
    std::vector<int> deps;        // BatchNode 配列内のインデックス
    double           weight = 1.0;// 推定所要秒。進捗の重み付けに使う
};

enum class BatchFailurePolicy { ContinueOthers, StopBranch, StopBatch };

struct BatchEstimate
{
    int    taskCount        = 0;   // 実際に投入されるタスク数
    int    skippedCount     = 0;   // 仕様が変わっていないためスキップ
    int    notApprovedCount = 0;   // 承認されていないため対象外
    int    blockedCount     = 0;
    int64_t estimatedSeconds = 0;
    double  estimatedCostUsd = 0.0;
    qint64  uploadBytes      = 0;
    QStringList remoteEndpoints;   // 同意ダイアログに列挙する送信先
    int     longestChainLength = 0;// 直列化される深さ
    QStringList warnings;
};

class StoryboardBatchJob : public QObject
{
    Q_OBJECT
public:
    struct Options {
        BatchFailurePolicy failurePolicy = BatchFailurePolicy::StopBranch;
        bool   includeRough       = false;   // ラフ生成モード
        bool   forceRegenerate    = false;   // dirtiness を無視して全生成
        std::set<OutputRole> roles;          // 空 = 全役割
    };

    /// 副作用なし。ダイアログの見積り表示に使う。
    static BatchEstimate plan(const std::vector<QUuid>& cutIds,
                              const Project& project,
                              const Options& opt);

    QUuid  start(const std::vector<QUuid>& cutIds, const Options& opt);  // batchId
    void   cancel();
    double aggregatedProgress() const;

signals:
    void nodeStateChanged(QUuid cutId, QUuid bindingId, OutputState s);
    void progressChanged(double p);
    void finished(int succeeded, int failed, int skipped);
};

} // namespace yave::ai
```

### 13.6.1 依存 DAG

エッジは 2 種類ある。

1. **連続性エッジ**: `Continuity::mode` が `FromPreviousEnd` / `FromCutId` のとき、
   上流カットの `MainVideo` バインディングから当該カットの `MainVideo` へ
2. **出力間エッジ**: `OutputBinding::derivedFromBindingId`
   (字幕をナレーション生成物から STT で作る、リップシンクをナレーションから作る、など)

Kahn 法でトポロジカルソートする。循環を検出したら
`error.ai.storyboard.continuityCycle` を該当カット一覧付きで出し、バッチを投入しない。

#### 連続性チェーンはシーン内に閉じる

> **設計上の判断**: 「全カットを前カットに繋ぐ」を素直に実装すると
> mainVideo 役割が**完全に直列化**し、40 カットのバッチが 1 本の鎖になる。
> 並列度が 1 になればバッチにする意味が消える。したがって:
>
> - `ContinuityMode` の**既定は `FromBoardImage`** (ボード画像を I2V の開始参照にする)。
>   これは上流に依存しないので並列に走る
> - `FromPreviousEnd` は**シーン内でのみ**使う。
>   `Continuity::sceneBreak == true`、または `transitionOut != TransitionKind::Cut`
>   でチェーンを切る
> - 結果としてチェーン長は 3〜8 程度に収まり、シーン同士は並列に走る
>
> さらに、上流フレームの取得元は **`Committed` ではなく `Cached`** の成果物とする。
> 承認待ちでチェーン全体が止まると、バッチが実質使えない。

上流参照の抽出は既存の `ReferenceFrameExtractor` を使わず、
上流の `GeneratedAsset` を直接 FFmpeg でシークして最終フレームを PNG 化する
(タイムラインへ未反映のため合成できない)。

### 13.6.2 再生成の要否 (dirtiness)

バインディングごとに 2 本のハッシュを持つ。

| ハッシュ | 内容 |
|---|---|
| `specHash` | 解決済み `AiGenerationParams` の JSON (配置・状態フィールドを除く) + 合成プロンプト (ロック中はロック文字列) + ローカル参照ファイルの内容ハッシュ + 後処理オプション |
| `upstreamHash` | 上流ノードの `specHash` (再帰的) |

**スキップ条件** (すべて満たすときのみ):

1. `state == Committed`
2. `committedSpecHash == specHash`
3. `committedUpstreamHash == upstreamHash`
4. `committedClipIds` のクリップがすべて実在する
5. 参照しているアセットが解決できる

いずれかを満たさなければ `Stale` にする。`upstreamHash` を再帰的に持つことで、
**カット 3 を作り直すとカット 4〜6 も正しく dirty になる**。

事前ダイアログには「生成 N 件 / スキップ M 件 / 未承認 K 件 / ブロック L 件」を出す。

### 13.6.3 承認ゲート

既定では `CutStatus::Approved` のカットだけが本生成に進む。
「ラフ生成」モード (`Options::includeRough`) では `Rough` 以上を対象にし、
安価なモデル / 低ステップ / 低解像度のプリセットを当てる。

選択範囲に対象が 1 件も無ければ `error.ai.storyboard.noApprovedCuts` を出して
何も投入しない。

> この承認ゲートはワークフロー上の便利機能であると同時に、
> **L1 が生成したテキストが人間の目を通らずに課金対象の生成へ流れることを防ぐ
> セキュリティ境界**でもある (13.7.4 参照)。

### 13.6.4 実行レーンと優先度

現行設計の `QThreadPool pool_` (既定 2 スレッド、[7.4](07-ai-orchestrator.md)) は
本章の要求を満たさない。

- 20 カット × 3 出力 = 60 タスクが一度に積まれる
- GPU 律速 / ネットワーク律速 / CPU 律速で最適な並列度が正反対
- 14B クラスの動画モデルを 2 本同時に走らせると VRAM が破綻する
  (現行の「2 スレッド」はこれを許してしまう)

よってプールをレーンに分ける。

```cpp
// src/ai/AiGenerationOrchestrator.h
enum class ExecutionLane { LocalGpu, LocalCpu, Remote, Sidecar };
enum class TaskPriority  { Interactive = 1, Batch = 0 };   // QThreadPool::start(r, priority)

class AiGenerationOrchestrator
{
    // ...
    std::array<QThreadPool, 4> lanes_;
    // LocalGpu = 1
    // LocalCpu = max(1, idealThreadCount() / 2)
    // Remote   = 6 (エンドポイントごとにトークンバケットで追加制限)
    // Sidecar  = 設定値。既定 1
};
```

- レーンは `ProviderCapability` から導出する
  (`requiresNetwork` / `estimatedVramMb` / プロバイダ種別)
- `LocalGpu` を 1 にする理由は上記のとおり VRAM である
- リモートは `Endpoint::concurrencyLimit` で追加制限し、HTTP 429 は
  指数バックオフ付きリトライ (既存のリトライ経路にバックオフを規定する)
- 単発の `submit()` は `Interactive`、バッチは `Batch`。
  **60 タスクのバッチの後ろで単発生成が飢えない**

`ProviderCapability` に以下を追加する ([7.6](07-ai-orchestrator.md) に反映)。

```cpp
double  estimatedSecondsPerOutputSecond = 0.0;  // 進捗重み・時間見積り
double  costPerOutputSecondUsd          = 0.0;  // コスト見積り
int     concurrencyLimit                = 0;    // 0 = 無制限
bool    supportsStructuredOutput        = false;// L1 の JSON スキーマ強制に使う
int     maxInputTokens                  = 0;
```

### 13.6.5 進捗の集約

```
aggregatedProgress = Σ(wᵢ · pᵢ) / Σwᵢ        wᵢ = BatchNode::weight
```

重みは `estimatedSecondsPerOutputSecond × 出力秒数`。

> 均等重みにすると 90 秒かかる T2V と 2 秒で終わる TTS が同じ扱いになり、
> 進捗バーが長時間ぴくりとも動かない区間ができる。ユーザーは固まったと判断する。

### 13.6.6 部分失敗

既定は `StopBranch`。

- 失敗したノードの**連続性の子孫**は `Blocked` にする
  (誤った参照フレームから生成しても捨てることになり、時間と課金の無駄)
- 独立したブランチは走り続ける
- バッチは `CompletedWithErrors` で終了し、タスクパネルに
  「失敗したカットのみ再実行」を出す

### 13.6.7 バッチと Undo

**バッチを 1 つのマクロコマンドにしない。**

コミットは非同期に到着し、その間にユーザーは別の編集をしている。
1 マクロにすると、ユーザー操作をまたいだ範囲がひとつの Undo ステップになり、
`QUndoStack` の線形性が壊れる。

したがって:

- **1 カットのコミット = 1 Undo ステップ**を維持する
  (`design.md` §3.2 の「気に入らなければ戻す」要件は、カット単位で満たされる)
- タスクパネルの「このバッチのコミットをすべて取り消す」は、
  **スタックを巻き戻すのではなく逆コマンドのマクロを新規 push する**

### 13.6.8 事前見積りと同意

`BatchGenerateDialog` は `plan()` の結果を表示してから投入する。

- 生成 / スキップ / 未承認 / ブロック 件数
- 推定所要時間、推定コスト、アップロード総量
- **送信先リモートエンドポイントの一覧**

[7.6.2](07-ai-orchestrator.md) のリモート送信同意は、バッチではタスクごとではなく
**バッチ単位で 1 回**取る。ダイアログには全エンドポイントを列挙する。

## 13.7 L1 ストーリーボードプランニング

### 13.7.1 生成種別の追加

```cpp
// src/core/ai/AiGenerationParams.h
enum class GenerationKind
{
    Video, Audio, Subtitle, Mask, EffectMetadata, Image,
    Storyboard          // 追加: カット列そのものを生成する
};

enum class GenerationPurpose { Commit, AnimaticPreview };

enum class PlanFitMode { ScaleToFit, KeepModelDurations, TrimToRange };
```

`AiGenerationParams` への追加:

```cpp
// ---------------- Storyboard (L1) ----------------
QString              storyboardRequest;          // ユーザーの自然言語要求
int                  targetCutCount = 0;         // 0 = モデルに委ねる
std::set<OutputRole> desiredRoles;               // 空 = モデルに委ねる
int                  planSchemaVersion = 1;      // 契約バージョン
bool                 includeExistingCutsAsContext = false;
PlanFitMode          planFitMode = PlanFitMode::ScaleToFit;

// ---------------- 共通 ----------------
GenerationPurpose    purpose = GenerationPurpose::Commit;
QUuid                batchId;                    // バッチに属する場合
struct CutRef { QUuid cutClipId; QUuid bindingId; };
std::optional<CutRef> cutRef;
```

プロバイダは `RemoteHttpProvider` (`Protocol::OpenAiCompatible`、または
`Protocol::Custom` + リクエストテンプレート) か `SidecarProvider` (ローカル LLM)。
`ProviderCapability::supportsStructuredOutput` があれば
`response_format: {"type":"json_schema","strict":true}` でスキーマを強制する。

### 13.7.2 契約 JSON

スキーマは `resources/ai/storyboard_plan.schema.json` としてリソースに同梱し、
`planSchemaVersion` でバージョン管理する。

```json
{
  "planSchemaVersion": 1,
  "storyBible": {
    "artStyle": "水彩調のアニメーション、柔らかい光",
    "negativePrompt": "低品質, 文字, 余分な指",
    "characters": [
      { "key": "aoi", "name": "葵",
        "appearance": "黒髪ショート、紺のセーラー服",
        "promptFragment": "short black hair, navy sailor uniform",
        "voiceHint": "young female" }
    ],
    "locations": [
      { "key": "rooftop", "name": "校舎の屋上",
        "promptFragment": "school rooftop, chain-link fence, morning haze" }
    ]
  },
  "tracks": [
    { "role": "mainVideo", "name": "AI Video" },
    { "role": "narration", "name": "ナレーション" },
    { "role": "subtitle",  "name": "字幕" }
  ],
  "cuts": [
    {
      "index": 1,
      "slug": "屋上・朝",
      "durationSeconds": 8.0,
      "description": "葵が屋上のフェンス越しに街を見下ろす。風で髪が揺れる。",
      "dialogue": "……今日で、最後か。",
      "mood": "静か / 寂しい",
      "characterKeys": ["aoi"],
      "locationKey": "rooftop",
      "camera": { "size": "medium", "angle": "eyeLevel", "movement": "slowPushIn" },
      "transitionOut": "dissolve",
      "sceneBreak": false,
      "outputs": ["mainVideo", "narration", "subtitle"]
    }
  ]
}
```

> **`durationSeconds` はシステム内で唯一許される `double` 秒である。**
> `design.md` §3.1 は `double` 秒での時刻保持を禁じているが、モデルは
> 60000/1001 のタイムベースで推論できない。よって**外部境界でのみ秒を受け取り、
> パーサで `secondsToFrames(RoundMode::Nearest)` へ変換し、秒のまま保持しない**。
> [6 章](06-subtitle-engine.md)の SRT 取り込みが秒を受ける前例と同じ扱いである。

### 13.7.3 検証器 — モデル出力は信頼できないデータである

```cpp
// src/ai/StoryboardPlanParser.h
namespace yave::ai {

struct StoryboardPlan
{
    StoryBible                 bibleAdditions;
    std::vector<TrackRequest>  tracks;
    std::vector<CutDraft>      cuts;      // range はフレームへ変換済み
    QStringList                warnings;
};

class StoryboardPlanParser
{
public:
    struct Result { bool ok; StoryboardPlan plan; QString errorKey; QStringList details; };

    Result parse(const QByteArray& raw,
                 const AiGenerationParams& params,
                 const StoryBible& existingBible,
                 const TimeRange& plannedRange,
                 const Rational& timebase) const;
};

} // namespace yave::ai
```

検証規則:

1. **パース前**にサイズ上限を適用する。応答 1 MiB、カット 500 件。
   超過は `error.ai.storyboard.tooLarge`
2. 決定的な修復は**「先頭と末尾のコードフェンス 1 組の除去」のみ**。
   それ以外の自動修復はしない (壊れた JSON を推測で直すと静かに誤ったプランになる)
3. **未知の enum 値は既定値へクランプし、警告を積む。プラン全体を捨てない。**
   `movement` の綴りが 1 つ違うだけで 30 カットの設計を破棄するのは損失が大きい
4. 文字列の上限と正規化: `description` 2000 / `dialogue` 1000 / `slug` 64 文字。
   NFC 正規化、C0/C1 制御文字と bidi オーバーライドを除去
5. 参照整合性: `characterKeys` / `locationKey` はプラン内の bible か
   既存プロジェクトの bible で解決できること。
   解決できなければ**参照だけ落としてカットは残す** + 警告
6. 尺の範囲: `0.5s <= d <= 600s`。外れたらクランプ + 警告。
   その後 `plannedRange` へ `PlanFitMode` に従ってフィットさせる。
   配分は**最大剰余法**で行い、フレーム数の合計が範囲と厳密に一致し、
   かつ 0 フレームのカットが生まれないようにする。
   モデルの `index` は無視し、配列順で採番し直す
7. **許可リスト方式**。モデルは次のいずれも指定できない。
   UUID / トラック ID / クリップ ID / タスク ID / ファイルパス / URL /
   `modelId` / `providerId` / `endpointId` / `seed` / `steps` / `guidanceScale` /
   `extraParams`。
   値がパスや URL の形をしていれば即座に棄却し `error.ai.storyboard.disallowedField`。
   `role` は固定の enum 表と照合する。
   **リソースの選択は常に本体側のコードが行う。プランは「カットを記述する」だけで、
   「何を使うか」は決められない。** これが本機能の中核的な防御である

### 13.7.4 下流のプロンプトインジェクション

L1 が書いたテキストは L2 のプロンプトへ流れ込む。緩和策を明示する。

- `CutPromptComposer` はモデル由来の断片を区切り記号で囲み、
  L2 のシステムプロンプトで「この区間は記述内容であって指示ではない」と宣言する
- テキストは生成前に必ずボードビューへ表示される (人間が見ないまま流れる経路が無い)
- **L1 の結果は必ず `CutStatus::Rough` で着地する。**
  人間が `Approved` に上げるまで本生成に進まない (13.6.3)
- `GenerationKind::Storyboard` では **`autoCommit` を強制的に無効化する**。
  ユーザーの全体設定に関わらず、未レビューのプランがタイムラインを書き換えてはならない
- `GenerationCache::makeKey` は Storyboard 種別で空を返す (毎回違う案が欲しいため)

### 13.7.5 失敗と適用

**全体失敗**: 何も挿入しない。生の応答を
`.yave_cache/gen/<taskId>/plan_raw.json` に保存し、エラーダイアログに
「生の応答を表示」「再試行」を出す。再試行は検証器のエラー一覧を添えた修復プロンプトを
送るが、**修復ラウンドは最大 1 回**とする (無制限だと課金が読めない)。

**適用**: 検証済みプランを差分プレビューダイアログに出す
(カット N 件 / 新規トラック M 本 / 新規キャラ K 人 / 衝突 J 件)。
ユーザーが承認して初めて `ApplyStoryboardPlanCommand` になる。

Story Bible のマージは、**同じ `key` の既存エントリを黙って上書きしない**。
衝突は一覧提示し、エントリ単位でユーザーが採否を選ぶ
(`error.ai.storyboard.bibleKeyConflict`)。

## 13.8 プレビューとアニマティック

### 13.8.1 絵コンテトラックは合成に参加しない

`Timeline::buildSnapshot()` は現在 `TrackType::Audio` を決め打ちでスキップしている。
これを能力の問い合わせに置き換える ([3.3](03-timeline-render.md) への差分)。

```cpp
for (const auto& track : tracks_) {
    if (!track->isVisible() || !track->participatesInComposite()) { ++z; continue; }
    // ...
}
```

置き換えないと `CutClip` に実レイヤーの生成を要求してしまう。

### 13.8.2 PreviewMode

```cpp
enum class PreviewMode { Normal, Animatic, AnimaticOnly };
```

`PlaybackController` (およびレンダリング設定) が保持する。
**`Timeline` には持たせない。** これはビュー状態であってプロジェクト状態ではなく、
Undo スタックにもプロジェクト JSON にも入れてはならない。

**フォールバック規則**: `Animatic` では、カットの `MainVideo` バインディングに
そのフレームを覆うコミット済みクリップが**無いときだけ**、カットがレイヤーを出す。
**既に生成済みの素材が常に勝つ。**
`AnimaticOnly` は生成済みでもボードを強制表示する (ボードのレビュー用)。

### 13.8.3 既存 RenderSnapshot への載せ方

新しい描画経路を作らず、`LayerItem` の既存の仕組みへ写す。

| カットの状態 | `LayerItem::source` |
|---|---|
| ボード画像あり | `AssetLibrary` に登録し、素の `VideoSourceRef{assetId, sourceFrame:0}` (静止画は既にこの経路) |
| キャプション (カット番号 / セリフ / カメラワーク) | `SubtitleRenderRef` で `CutClip::animaticCaption()` を指す。`SubtitleRenderer::buildFrame` をそのまま再利用 |
| ボード画像なし | 新規の `PlaceholderCardRef` |

```cpp
// src/core/RenderSnapshot.h
struct PlaceholderCardRef
{
    QString titleText;        // 「カット 12 (未生成)」
    QString bodyText;         // プロンプト先頭 / セリフ抜粋
    double  progress = -1.0;  // 0 未満なら進捗バーを描かない
    QColor  accent;           // 承認状態の色
};

std::variant<std::monostate,
             VideoSourceRef,
             SubtitleRenderRef,
             GeneratedSourceRef,
             PlaceholderCardRef>  source;
```

> `PlaceholderCardRef` を足す価値は、既存の穴も同時に塞ぐ点にある。
> [7.8.2](07-ai-orchestrator.md) の `AiPlaceholderClip::makeLayerItem` は
> 「半透明のプレースホルダ画像を返す」とだけ書かれており、
> **それがどうやってコンポジタへ届くかが未定義**だった。
> 1 つの variant 選択肢が、生成中プレースホルダと未生成カットの両方を賄う。

`SubtitleRenderRef` の生存期間の契約は既存のもの
([12 章](12-snippets.md)のコンポジタが `*src.clip` を参照する) と同じであり、
`animaticCaption()` は `CutClip` が所有し続ける。新しい危険は導入しない。

### 13.8.4 音声側 (TTS プレビュー)

- `GenerationPurpose::AnimaticPreview` のナレーションは
  必ずローカル TTS (`OnnxLocalProvider` の Piper / VITS) で生成し、
  `.yave_cache/animatic/<cutId>.wav` に置く。
  **トラックにはコミットしない。** リモートへも送らない
- 再生は `AudioRenderGraph` にプレビュー専用の `TrackNode`
  (`isPreviewOnly = true`) を、`PreviewMode != Normal` のときだけ構築する。
  グラフは PCM の事前ロードを要求するが ([5.3.2](05-audio-engine.md))、
  カット単位のナレーションは小さいので問題にならない
- TTS がカット尺を超えたら、ボードビューとタイムラインに超過インジケータを出し、
  `FitCutToDialogueCommand` (Undo 可能) を提示する。
  **黙ってタイムラインを伸ばさない**

### 13.8.5 エクスポート

`ExportJob` は `PreviewMode::Normal` を強制する。
アニマティックはあくまでプレビューであり、書き出しには含めない。

エクスポート範囲にコミット済み出力を持たないカットがあれば、
`warning.export.uncommittedCuts` を該当カット番号付きで出す。

## 13.9 Undo コマンド

すべて `src/core/commands/` に置き、既存の `<動詞><名詞>Command` 命名と
`UndoCommandBase` 派生に従う。

| コマンド | redo | undo |
|---|---|---|
| `AddCutCommand` | 絵コンテトラックへ `CutClip` を挿入。既定で後続カットをリップル | 挿入を取り消し、リップルを復元 |
| `RemoveCutCommand` | カットを除去 (オプションで出力クリップも除去) | 保持した `shared_ptr` から復元 |
| `ReorderCutsCommand` | 選択カットの `TimeRange` 群を入れ替える。尺は維持しリップル | 変更前の `TimeRange` ベクタを丸ごと復元 |
| `EditCutSpecCommand` | 構造化フィールドの変更 (複数カット一括可) | 旧値を復元 |
| `SetCutStatusCommand` | 承認状態の変更 (複数選択一括) | 旧状態を復元 |
| `BindCutOutputCommand` | `OutputBinding` の追加 / 削除 / 再解決。必要なら子コマンドで `AddTrackCommand` を実行 | 子コマンドを含め逆順に取り消し |
| `EditStoryBibleCommand` | Story Bible の追加 / 編集 / 削除 | 旧 Bible を復元 |
| `ApplyStoryboardPlanCommand` | L1 結果の一括適用 (トラック生成 → カット挿入 → Bible マージ) を親子コマンドで | 全体を 1 手で取り消し |
| `FitCutToDialogueCommand` | カット尺を TTS 尺に合わせてリップル | 旧尺を復元 |

`UndoCommandBase::CommandId` に追加する。

```cpp
IdEditCutSpec    = 7,   // 同一 cutId + fieldKey で mergeWith する (スライダ操作の連続をまとめる)
IdEditStoryBible = 8,
IdSetCutStatus   = 9,
```

### 13.9.1 コミットは既存コマンドを拡張する

新規に `CommitCutOutputCommand` を作らず、
`CommitGeneratedAssetCommand` に任意の `CutRef` を持たせる。

```cpp
class CommitGeneratedAssetCommand : public QUndoCommand
{
public:
    CommitGeneratedAssetCommand(Timeline* tl,
                                const QUuid& taskId,
                                std::vector<GeneratedAsset> assets,
                                const ai::AiGenerationParams& params,
                                std::optional<ai::CutRef> cutRef = std::nullopt);

    void redo() override;   // cutRef があれば、クリップ挿入に加えて
                            // resolvedTrackId / committedClipIds /
                            // committedSpecHash / committedUpstreamHash / state を書き戻す
    void undo() override;   // バインディングの直前スナップショットを復元
};
```

> **分けない理由**: クリップの挿入とバインディングへの書き戻しは原子的でなければならない。
> 2 つのコマンドに分けると、「クリップは入ったがバインディングは未更新」という
> 半端に undo された状態が到達可能になり、次のバッチが同じものを二重生成する。

### 13.9.2 実行中タスクを Undo したとき

`AddCutCommand` を undo したのに、そのカットのタスクがまだ走っているという状況は
必ず起きる。規則:

- 成果物は**捨てない**。タスクは `Cached` のまま残す
- 状態メッセージは `status.ai.storyboard.orphanedResult`
- 後で redo されたら、カット ID で再リンクする
- 破棄はユーザーの明示操作 (`discard`) かキャッシュの LRU 削除でのみ行う

### 13.9.3 プレースホルダを置かない

`InsertPlaceholderCommand` ([7.4.1](07-ai-orchestrator.md)) は
**`TrackType::Storyboard` のトラックをスキップする**。
`CutClip` そのものがプレースホルダの役割を果たしているため、
その上に別のプレースホルダを重ねる必要がない
(そもそもクリップ非重複の不変条件に反する)。

出力トラック側には従来どおりプレースホルダを置く。

## 13.10 JSON スキーマ

`kCurrentSchemaVersion` を **1 から 2** へ上げる。

### 13.10.1 ルートへの追加

```json
{
  "schemaVersion": 2,

  "storyBible": {
    "artStyle": "水彩調のアニメーション、柔らかい光",
    "negativePrompt": "低品質, 文字, 余分な指",
    "promptPrefix": "",
    "promptSuffix": "",
    "characters": [
      {
        "id": "3b1e7c22-0000-4000-8000-000000000001",
        "key": "aoi",
        "name": "葵",
        "appearance": "黒髪ショート、紺のセーラー服",
        "personality": "内向的で静か",
        "promptFragment": "short black hair, navy sailor uniform",
        "voiceId": "piper-ja-female-1",
        "referenceImage": {
          "source": "filePath",
          "filePath": "assets/bible/aoi.png",
          "strength": 1.0
        }
      }
    ],
    "locations": [
      {
        "id": "9c22ab10-0000-4000-8000-000000000002",
        "key": "rooftop",
        "name": "校舎の屋上",
        "description": "朝の光。フェンス越しに街が見える",
        "promptFragment": "school rooftop, chain-link fence, morning haze"
      }
    ],
    "roleDefaults": {
      "mainVideo": {
        "modelId": "wan2.2-i2v-14b",
        "steps": 30,
        "outputResolution": { "width": 1280, "height": 720 },
        "outputFrameRate": { "num": 1, "den": 30 }
      },
      "narration": { "modelId": "piper-ja", "targetLufs": -16.0 }
    },
    "promptTemplates": {
      "mainVideo": "{{artStyle}}. {{location}}. {{characters}}. {{description}} {{camera}} {{mood}}",
      "narration": "{{dialogue}}"
    }
  }
}
```

### 13.10.2 トラックへの追加

```json
"tracks": [
  {
    "id": "eeee0000-0000-4000-8000-00000000000a",
    "name": "絵コンテ",
    "type": "storyboard",
    "visible": true,
    "locked": false,
    "height": 96,
    "color": "#7a5f3a",
    "aiRole": "",
    "storyboardTrackId": null,
    "roleDefaults": { "mainVideo": { "steps": 24 } },
    "clips": [ /* 13.10.3 の CutClip */ ]
  },
  {
    "id": "gggg0000-0000-4000-8000-00000000000b",
    "name": "AI Video",
    "type": "video",
    "aiRole": "mainVideo",
    "storyboardTrackId": "eeee0000-0000-4000-8000-00000000000a",
    "clips": [ /* VideoClip */ ]
  }
]
```

`aiRole` / `storyboardTrackId` / `roleDefaults` は全トラック型で任意。
既定は `""` / `null` / `{}`。

### 13.10.3 CutClip (新 9.4.5)

```json
{
  "id": "cut00001-0000-4000-8000-000000000010",
  "type": "cut",
  "range": { "start": 0, "duration": 480 },

  "label": "",
  "slug": "屋上・朝",
  "description": "葵が屋上のフェンス越しに街を見下ろす。風で髪が揺れる。",
  "dialogue": "……今日で、最後か。",
  "mood": "静か / 寂しい",

  "characterIds": ["3b1e7c22-0000-4000-8000-000000000001"],
  "locationId": "9c22ab10-0000-4000-8000-000000000002",

  "camera": { "size": "medium", "angle": "eyeLevel", "movement": "slowPushIn", "note": "" },
  "transitionIn": "cut",
  "transitionOut": "dissolve",

  "board": {
    "origin": "userFile",
    "assetId": "b0010000-0000-4000-8000-000000000020",
    "sourceTrackId": null,
    "sourceFrame": 0,
    "generatedByTaskId": null
  },

  "continuity": {
    "mode": "fromBoardImage",
    "fromCutId": null,
    "strength": 0.9,
    "sceneBreak": false
  },

  "status": "inReview",
  "reviewNote": "",

  "paramPatch": { "seed": 987654321 },
  "biblePatch": {},

  "outputs": [
    {
      "id": "ob100000-0000-4000-8000-000000000030",
      "role": "mainVideo",
      "roleTag": "",
      "enabled": true,
      "resolveMode": "auto",
      "resolvedTrackId": "gggg0000-0000-4000-8000-00000000000b",
      "trackNameHint": "AI Video",
      "derivedFromBindingId": null,
      "leadInFrames": 0,
      "leadOutFrames": 15,
      "paramPatch": { "modelId": "wan2.2-i2v-14b" },
      "promptLock": {
        "locked": false,
        "prompt": "",
        "negativePrompt": "",
        "lockedAgainstHash": ""
      },
      "lastTaskId": "task0000-0000-4000-8000-000000000040",
      "committedClipIds": ["c0010000-0000-4000-8000-000000000050"],
      "committedSpecHash": "sha256:ab12cd34",
      "committedUpstreamHash": "sha256:cd34ef56",
      "state": "committed"
    },
    {
      "id": "ob200000-0000-4000-8000-000000000031",
      "role": "narration",
      "resolveMode": "auto",
      "resolvedTrackId": "hhhh0000-0000-4000-8000-00000000000c",
      "derivedFromBindingId": null,
      "paramPatch": { "voiceId": "piper-ja-female-1" },
      "committedClipIds": ["c0020000-0000-4000-8000-000000000051"],
      "state": "committed"
    },
    {
      "id": "ob300000-0000-4000-8000-000000000032",
      "role": "subtitle",
      "resolveMode": "auto",
      "resolvedTrackId": null,
      "derivedFromBindingId": "ob200000-0000-4000-8000-000000000031",
      "state": "notGenerated"
    }
  ],

  "generatedByTaskId": null
}
```

### 13.10.4 スキーマバージョンとマイグレーション

追加のみで破壊的変更は無いが、それでもバージョンを上げる。理由:
**v1 のライタが v2 のプロジェクトを保存すると、ルートの `storyBible` を丸ごと落とし、
`"type":"storyboard"` のトラックを別の型として書き戻してしまう**からである。

- `migrations()` に `{ 1, &migrate_1_to_2 }` を追加する。本体は既定値の補完のみ
  (`storyBible = {}`、各トラックに `aiRole = ""` / `storyboardTrackId = null`)
- **`unknownFields` の保持を `Clip` だけでなく `Track` とプロジェクトルートへ拡張する。**
  現行 9.11.2 は `Clip` にしか規定がなく、これが無いと前方互換の約束が
  新しいデータをカバーしない
- 未知の `type` 文字列は `enumFromString` のフォールバックに任せず
  **`TrackType::Unknown`** にする。`Unknown` のトラックは合成せず、
  クリップを受け付けず、UI で読み取り専用にし、保存時は元の JSON をそのまま書き戻す

`EnumMap` の特殊化を追加する ([9.9](09-project-io.md))。

| enum | 文字列 |
|---|---|
| `TrackType` | 既存 + `storyboard`, `unknown` |
| `ClipType` | 既存 + `cut` |
| `OutputRole` | `mainVideo`, `mainVideoB`, `overlay`, `narration`, `bgm`, `se`, `subtitle`, `mask` |
| `TrackResolveMode` | `auto`, `existing`, `alwaysNew` |
| `OutputState` | `notGenerated`, `queued`, `running`, `cached`, `committed`, `failed`, `stale`, `blocked` |
| `CutStatus` | `notStarted`, `rough`, `inReview`, `approved` |
| `ContinuityMode` | `none`, `fromBoardImage`, `fromPreviousEnd`, `fromCutId` |
| `ShotSize` | `unspecified`, `extremeWide`, `wide`, `full`, `medium`, `closeUp`, `extremeCloseUp` |
| `CameraAngle` | `unspecified`, `eyeLevel`, `high`, `low`, `birdsEye`, `wormsEye`, `dutch` |
| `CameraMovement` | `unspecified`, `fixed`, `panLeft`, `panRight`, `tiltUp`, `tiltDown`, `dolly`, `slowPushIn`, `pullOut`, `handheld`, `crane`, `follow` |
| `TransitionKind` | `cut`, `dissolve`, `fadeToBlack`, `fadeFromBlack`, `wipe`, `matchCut` |
| `BoardImage::Origin` | `none`, `userFile`, `generated`, `timelineFrame` |
| `GenerationKind` | 既存 + `storyboard` |
| `GenerationPurpose` | `commit`, `animaticPreview` |
| `PlanFitMode` | `scaleToFit`, `keepModelDurations`, `trimToRange` |

### 13.10.5 aiTasks の拡張と永続化ポリシー

`aiTasks` の各要素に追加する。

```json
{
  "batchId": "batch000-0000-4000-8000-000000000060",
  "purpose": "commit",
  "cutRef": {
    "cutClipId": "cut00001-0000-4000-8000-000000000010",
    "bindingId": "ob100000-0000-4000-8000-000000000030"
  }
}
```

**永続化ポリシー (新規)**: プロジェクト JSON に書くのは
**`Cached` または `Committed` で、かつ生きた `OutputBinding` から参照されているタスクだけ**
とする。それ以外は `.yave_cache/tasks.json` にのみ置く。

> 100 カットのプロジェクトを 5 回作り直すと、素直に全部書けば数千件のタスクレコードが
> 積み上がり、[9.14](09-project-io.md) のファイルサイズ予算 (1000 クリップで 3MB) を破る。
> UI に「生成履歴を整理」を用意し、参照されていないタスクを一括削除できるようにする。

### 13.10.6 JsonKeys への追加

```
kStoryBible, kCharacters, kLocations, kRoleDefaults, kPromptTemplates,
kArtStyle, kPromptPrefix, kPromptSuffix, kPromptFragment, kAppearance,
kAiRole, kStoryboardTrackId,
kSlug, kLabel, kDescription, kDialogue, kMood, kCamera,
kTransitionIn, kTransitionOut, kBoard, kContinuity, kStatus, kReviewNote,
kOutputs, kRole, kRoleTag, kResolveMode, kResolvedTrackId, kTrackNameHint,
kDerivedFromBindingId, kLeadInFrames, kLeadOutFrames,
kParamPatch, kBiblePatch, kPromptLock, kLockedAgainstHash,
kCommittedClipIds, kCommittedSpecHash, kCommittedUpstreamHash,
kBatchId, kCutRef, kPurpose
```

## 13.11 UI 設計

### 13.11.1 ボードビュー (`StoryboardBoardPanel.qml`)

タイムラインと同じデータを、絵コンテ表として見るパネル。

```
+- 絵コンテ ------------------------------------------------------------[絞込 ▼]-+
|  +---------------+  +---------------+  +---------------+                        |
|  | 1  屋上・朝    |  | 2  教室       |  | 3  廊下       |                        |
|  | +-----------+ |  | +-----------+ |  | +-----------+ |                        |
|  | |  (board)  | |  | |  (board)  | |  | |   未生成   | |                        |
|  | +-----------+ |  | +-----------+ |  | +-----------+ |                        |
|  | 8.0s  M/EL/PI |  | 4.0s  CU/EL/F |  | 6.0s  W/L/PAN |                        |
|  | 「今日で、…」 |  | 「おはよう」  |  |               |                        |
|  | [V][N][字]    |  | [V][N][字]    |  | [V][N]        |                        |
|  | ● 確認中      |  | ● 承認済      |  | ○ ラフ        |                        |
|  +---------------+  +---------------+  +---------------+   [ + ]                |
+------------------------------------------------------------------------------+
```

- カード = 1 カット。通し番号 (または `label`)、ボードサムネイル、尺、
  カメラワーク略記、セリフ抜粋、役割バッジ (V / N / BGM / SE / 字)、承認ステータス
- 役割バッジの色が `OutputState` を示す
  (未生成 = 灰、生成中 = 青、Cached = 黄、Committed = 緑、Stale = 橙、Failed = 赤)
- ドラッグで並び替え → `ReorderCutsCommand`
- カード間の `+` で挿入 → `AddCutCommand`
- 選択は `SelectionModel` に bind するため、タイムライン側の選択と自動的に一致する

### 13.11.2 カットインスペクタ (`CutInspector.qml`)

- 構造化フィールドの編集 (見出し / ト書き / セリフ / ムード / カメラワーク /
  キャラ / ロケ / トランジション / ボード画像 / 連続性 / 承認状態)
- 各フィールドに**カスケードインジケータ**を付ける。
  継承中は小さなドットを表示し、ツールチップに「継承元: Story Bible」を出す。
  クリックで継承に戻す (`patch.remove(key)`)
- 出力バインディング一覧: 役割 / 有効 / 配置先トラック (コンボ) / 状態 / 個別パラメータ
- 合成プロンプトパネル: 読み取り専用表示 + provenance ガター + ロックトグル +
  陳腐化バッジ

### 13.11.3 タイムライン上の表示 (`CutClipItem.qml`)

- ボードサムネイルの帯 + カット番号 + 承認状態のカラーバンド
- バインディング数のアイコン
- 連続性で繋がっている隣接カットの間に鎖の矢印を描く
  (どこでチェーンが切れているかが一目で分かる)

### 13.11.4 その他の新規 QML

| ファイル | 用途 |
|---|---|
| `qml/ai/StoryBibleEditor.qml` | キャラクター / ロケーション / 画風の編集 |
| `qml/ai/StoryboardPlanDialog.qml` | L1 の要求入力と、検証済みプランの差分プレビュー |
| `qml/ai/BatchGenerateDialog.qml` | `BatchEstimate` の表示とリモート送信同意 |

### 13.11.5 既存 AI UI との関係

既存の 2 つ ([7.10](07-ai-orchestrator.md)) は残し、役割を明確に分ける。

**`AiGenerateDialog.qml`**

- 単発の区間生成用としてはそのまま残る
- カットから開くときは**1 つの `OutputBinding` にスコープされた**モードで開く。
  ヘッダに「カット 3 / ナレーション」と出し、値はカスケード解決済みで前埋めする
- バインディングが所有するフィールド (`targetTrackId` / `createNewTrack` / `range`)
  は**グレーアウトして編集不可**にする (13.14.6)
- 「この設定をこのカットに保存」ボタンは、素のタスクを投げるのではなく
  `paramPatch` への `EditCutSpecCommand` を発行する

**`AiTaskListPanel.qml`**

- `batchId` でグループ化する。親行は折りたためるバッチ行で、重み付き集約進捗を出す
- 子行は「カット 3 / 映像」のラベル。クリックでそのカットへナビゲートする

### 13.11.6 新規 C++ UI 層

| クラス | 役割 |
|---|---|
| `src/app/controllers/StoryboardController.h` | L1 リクエスト、バッチジョブ、ボードモデルの所有。`AiController` を肥大化させない |
| `src/app/models/CutListModel.h` | ボードビュー用の `QAbstractListModel` |
| `src/app/models/SelectionModel.h` | 13.5 |

## 13.12 i18n キー

[10 章](10-i18n.md)の規約に従う。

```
error.ai.storyboard.tooLarge
error.ai.storyboard.invalidJson
error.ai.storyboard.schemaMismatch
error.ai.storyboard.disallowedField
error.ai.storyboard.continuityCycle
error.ai.storyboard.noApprovedCuts
error.ai.storyboard.bibleKeyConflict
error.ai.storyboard.emptyPlan

error.ai.cut.noOutputs
error.ai.cut.boundTrackMissing
error.ai.cut.trackTypeMismatch
error.ai.cut.zeroDuration

status.ai.storyboard.planning
status.ai.storyboard.batchRunning
status.ai.storyboard.orphanedResult
status.ai.storyboard.skippedUnchanged
status.ai.storyboard.blockedByUpstream

warning.export.uncommittedCuts
warning.ai.cut.dialogueOverflow
warning.ai.cut.promptLockStale
```

複数形の扱い (10 章の `%n` 規約):

```cpp
tr("Reorder %n cut(s)", "", n);
tr("%n cut(s) will be generated", "", n);
```

**追加する規約**: モデルへ渡すプロンプト文字列、および L1 が生成したテキストは
翻訳経路に載せない (13.4.3)。翻訳するのは UI ラベルとエラーメッセージだけである。

## 13.13 テスト計画

`yave_core` は FFmpeg / QRhi / ONNX に依存しないため、以下はすべて GPU 無しの CI で走る。

| テスト | 内容 |
|---|---|
| `tst_cutclip.cpp` | `CutClip` の往復シリアライズ、`clone()`、`specHash` の安定性と除外集合 |
| `tst_cutcascade.cpp` | 4 段マージの優先順位、provenance、`AiGenerationParams::toJson()` の全キーが往復すること、継承リセット |
| `tst_roletrackresolver.cpp` | 冪等性 (2 回解決してもトラックが増えない)、Z 配置の決定性、型不一致の検出 |
| `tst_batchdag.cpp` | トポロジカルソート、循環検出、`sceneBreak` でのチェーン分断、dirtiness のスキップ判定、上流の再帰的 dirty 伝播 |
| `tst_storyboardplan.cpp` | 契約 JSON の検証。壊れた JSON / 巨大応答 / 未知 enum / パス注入 / URL 注入 / 未解決キー / 尺のクランプと最大剰余法による配分 |
| `tst_promptcomposer.cpp` | テンプレート展開、未知プレースホルダ、ロックの陳腐化検出 |
| `tst_selectionmodel.cpp` | 選択モード、クリップ削除時の自己修復、`resolvedCutIds` の交差判定 |
| `tst_projectio.cpp` (拡張) | schemaVersion 1 → 2 のマイグレーション、`Unknown` トラックの原文保持 |

`tst_storyboardplan.cpp` には**敵対的な入力のケースを必ず含める**。
「モデルが `filePath` を指定してきた」「`modelId` を上書きしようとした」
「制御文字を混ぜてきた」を明示的にテストする。

## 13.14 既存章との整合、および設計上の判断の記録

### 13.14.1 `AiGenerated` は「マーカーにすぎない」

[3.2.4](03-timeline-render.md) の記述と本章は矛盾しない。
`AiGenerated` は無変更で残し、`Storyboard` を別型として追加した。
`Track::participatesInComposite()` の導入により、
「振る舞いは中身のクリップ型で決まる」というマーカー規則は文字通り真であり続ける。

### 13.14.2 `buildSnapshot` の決め打ち

`TrackType::Audio` のみをスキップする実装では `CutClip` にレイヤー生成を要求してしまう。
述語に置き換える (13.8.1)。

### 13.14.3 モジュール階層の違反 (既存の問題)

`AiPlaceholderClip` は `src/core/` にありながら `yave_ai` の
`AiGenerationParams` を保持している。一方 [2.2](02-directory-layout.md) は
`yave_ai -> yave_core` の一方向依存を定めている。`CutClip` がこれを構造的にする。

**解消**: 純データ型 (`AiGenerationParams`、その enum 群、`ImageReference`、
`VideoReference`) を `src/core/ai/AiGenerationParams.h` へ移す。
namespace `yave::ai` は維持する。これらは `TimeRange` / `Rational` / Qt Core にしか
依存しないので、`yave_core` の「GPU 無し CI で動く」性質は失われない。
`yave_ai` にはオーケストレータ / プロバイダ / キャッシュ / タスクが残る。

### 13.14.4 `GenerationKind::Storyboard` はメディアを生まない

既存パイプラインは `postProcess` がファイルを生み `commit` がクリップを挿入する前提だが、
L1 は JSON しか生まない。

**解消**: `GeneratedAsset::Type::Json` は既に存在する。コミット経路は
`ApplyStoryboardPlanCommand` へ分岐させ、`postProcess` は `plan_raw.json` の
保存以外は何もしない。`autoCommit` は強制無効、キャッシュ対象外 (13.7.4)。

### 13.14.5 キャッシュキーの除外が足りない

[7.8.1](07-ai-orchestrator.md) の `makeKey` は `targetTrackId` /
`createNewTrack` / `replaceExistingClips` の 3 つしか除外していない。

カットでは以下も除外しなければならない。

```
status, reviewNote, label, slug,
resolvedTrackId, trackNameHint, lastTaskId,
committedClipIds, committedSpecHash, committedUpstreamHash, state,
leadInFrames, leadOutFrames, batchId, cutRef, purpose
```

> **除外しないと、カットを「確認中」から「承認済」にしただけでキャッシュキーが変わり、
> 同じ内容の生成に再課金される。** 除外集合は `tst_cutclip.cpp` で固定する。

### 13.14.6 配置の真実が二重にある

`AiGenerationParams::targetTrackId` / `createNewTrack` / `replaceExistingClips` と
`OutputBinding` の解決規則が競合する。

**規則**: `params.cutRef` が設定されているとき、配置は `OutputBinding` が決める。
上記 3 フィールドは無視され、UI でもグレーアウトされる。
単発生成 (`cutRef` 無し) では従来どおり有効である。

### 13.14.7 トランジションとクリップ非重複の不変条件

[3.2.2](03-timeline-render.md) は同一トラック内のクリップの重なりを禁じている。
一方 `transitionOut: "dissolve"` は前後のカットの映像が重なることを要求する。

**解消**: 絵コンテトラック上のカット区間は**厳密に非重複のまま**にする
(カット表は時間の分割であるべきで、これは正しい制約である)。
重なりは**出力側の A/B ロール**で実現する。
`MainVideo` と `MainVideoB` を交互に割り当て、`leadOutFrames` で
出ていくクリップをもう一方のトラックへ伸ばす。
`OutputRole::MainVideoB` が存在する理由はこれである。

### 13.14.8 ボード並び替えの破壊性

`ReorderCutsCommand` は最大 5 本の出力トラックのコミット済みクリップを動かしうる。

**規則**: 既定は「出力クリップも一緒に動かす」だが、
確認ダイアログに明示のチェックボックスを置き、
Undo テキストに影響クリップ数を出す (`tr("Reorder %n cut(s)", "", n)`)。

> カードグリッドでのドラッグひとつで、5 本のトラックのメディアが黙って移動するのは
> この機能が犯しうる最悪の挙動である。必ず可視化する。

### 13.14.9 選択モデルが存在しなかった

12 章すべてに選択の定義が無く、[6.9.2](06-subtitle-engine.md) の
`addEffectToSelectedSubtitles()` の時点で既に穴だった。13.5 で最小限を定義した。

### 13.14.10 `aiTasks` の無制限な増加

13.10.5 の永続化ポリシーで対処する。

### 13.14.11 Story Bible の ID と `design.md` §3.3

参照は `QUuid id`、照合と表示は `key` の併用で両立させた (13.3.4)。

### 13.14.12 段階導入

一度に全部を作らなくてよい。実装順の推奨:

| 段階 | 範囲 | この段階だけで得られる価値 |
|---|---|---|
| 1 | `TrackType::Storyboard` / `CutClip` / `SelectionModel` / タイムライン表示 / JSON | 手書きの絵コンテをタイムライン上で作れる |
| 2 | `OutputBinding` / `RoleTrackResolver` / `ParamCascade` / 単一カットの生成 | 1 カットから必要トラックが生えて実クリップが載る |
| 3 | ボードビュー / Story Bible / プロンプト合成 | 絵コンテアプリとしての作業フローが成立する |
| 4 | `StoryboardBatchJob` / 実行レーン / dirtiness / 連続性 | 選択範囲の一括生成が実用になる |
| 5 | L1 プランニング | 自然言語から構成案が作れる |
| 6 | アニマティック再生 | 生成前に尺とテンポを検証できる |

段階 1〜2 の時点で「AIトラックに区間を作り、パラメータを設定し、選択して生成すると
必要なトラックが生えて実クリップが載る」という当初の要求は満たされる。
3 以降は絵コンテアプリからの発想の取り込みであり、価値は大きいが後追いでよい。
