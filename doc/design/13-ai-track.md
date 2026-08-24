\# 13. AIトラック (演出指示 / 絵コンテトラック)



\[← 目次に戻る](../design.md)



\---



\## 13.1 このトラックは何か



\### 13.1.1 位置づけ



\*\*AIトラック\*\*は、タイムライン上に「\*\*これから何を作るか\*\*」を並べるトラックである。

映像や音声そのものは載らない。載るのは\*\*カット単位の演出指示\*\* (`CutClip`) であり、

これは絵コンテ (storyboard) の 1 コマに相当する。



生成を実行すると、各カットは自分が必要とする\*\*出力トラック\*\* (映像 / ナレーション /

BGM / SE / 字幕) を解決または新規作成し、そこへ実クリップを配置する。

AIトラックは生成後も残り続け、\*\*再生成の source of truth\*\* になる。



```

&#x20; ┌─ Storyboard トラック (AIトラック) ───────────────────────────────┐

&#x20; │ \[カット1 屋上・朝 ] \[カット2 教室 ] \[カット3 廊下  ] \[カット4 …] │  ← 仕様。合成に参加しない

&#x20; └────────┬──────────────┬─────────────┬──────────────────────┘

&#x20;          │ 生成          │             │

&#x20;          v              v             v

&#x20; ┌─ Video (aiRole=mainVideo) ────────────────────────────────────┐

&#x20; │ \[  生成映像1  ] \[  生成映像2  ] \[  生成映像3  ]                  │

&#x20; └───────────────────────────────────────────────────────────┘

&#x20; ┌─ Subtitle (aiRole=subtitle) ──────────────────────────────────┐

&#x20; │ \[ セリフ1 ]      \[ セリフ2 ]     \[ セリフ3 ]                     │

&#x20; └───────────────────────────────────────────────────────────┘

&#x20; ┌─ Audio (aiRole=narration) ────────────────────────────────────┐

&#x20; │ \[ TTS1    ]      \[ TTS2    ]     \[ TTS3    ]                     │

&#x20; └───────────────────────────────────────────────────────────┘

```



\### 13.1.2 L1 と L2



AI へのリクエストは 2 階層ある。両方を本章で定義する。



| 階層 | 入力 | 出力 | 実行主体 |

|---|---|---|---|

| \*\*L1 プランニング\*\* | 自然言語の要求 (「3 分の商品紹介動画を作って」) | カット列 + 必要トラック構成 + Story Bible 草案 | LLM (`GenerationKind::Storyboard`) |

| \*\*L2 マテリアライズ\*\* | 1 カットの演出指示 | 映像 / 音声 / 字幕 の実メディア | \[7 章](07-ai-orchestrator.md)の既存パイプライン |



L1 は\*\*タイムラインへ直接メディアを置かない\*\*。カットを置くだけである。

L2 は既存の `AiGenerationOrchestrator` をそのまま使い、本章はその上に

「1 カット → N タスク」の展開と「複数カット → 依存順バッチ」を積む。



\### 13.1.3 なぜ `AiGenerated` を再利用しないのか



\[3.2.4](03-timeline-render.md) は `TrackType::AiGenerated` について次のように定めている。



> `AiGenerated` トラックは「生成物であることを UI 上で区別する」ためのマーカーであり、

> 振る舞いは中身のクリップ型で決まる。



この記述は\*\*正しく、変更しない\*\*。AIトラックはこれとは逆の性質を持つ。



| | `AiGenerated` | `Storyboard` (本章) |

|---|---|---|

| 何が載るか | 生成された実クリップ (Video/Audio/Subtitle) | `CutClip` のみ |

| 合成への参加 | する (中身のクリップ型に従う) | \*\*しない\*\* |

| 振る舞い | 無し (マーカー) | 有り (出力解決・バッチ生成・アニマティック) |

| 生成後の扱い | 通常トラックへドラッグ移動可能 | 移動しない。仕様として残る |



同一の enum 値に 2 つの意味を持たせると `Track::acceptsClip()` と

`Timeline::buildSnapshot()` の両方が「どちらの AiGenerated か」を判別する必要が生じ、

マーカー規則が静かに壊れる。よって\*\*別の enum 値を追加する\*\*。



> \*\*既存プロジェクトへの影響はない。\*\* `AiGenerated` の意味論も JSON 表現も一切変わらない。



\## 13.2 用語



| 用語 | 意味 |

|---|---|

| \*\*カット\*\* | AIトラック上の 1 区間 (`CutClip`)。絵コンテの 1 コマ。演出指示の単位 |

| \*\*絵コンテトラック / AIトラック\*\* | 本書では同義。`TrackType::Storyboard` のトラック |

| \*\*出力バインディング\*\* | 1 カットが生む 1 つの成果物の仕様 (`OutputBinding`)。役割・配置先・パラメータ・生成状態を持つ |

| \*\*役割 (role)\*\* | 出力の種別 (`OutputRole`)。mainVideo / narration / bgm / se / subtitle / mask など |

| \*\*Story Bible\*\* | プロジェクト単位の設定集。キャラクター・ロケーション・画風・ネガティブ。全カットのプロンプトへカスケードする |

| \*\*アニマティック\*\* | 未生成カットをボード画像 + TTS プレビューで仮再生すること。絵コンテ動画 |

| \*\*承認状態\*\* | カットの進行状態 (`CutStatus`)。未着手 / ラフ / 確認中 / 承認済み |

| \*\*仕様ハッシュ\*\* | 出力バインディング 1 件の生成仕様を要約した SHA-256。再生成要否の判定に使う |

| \*\*プラン\*\* | L1 が生成する JSON。カット列とトラック構成の設計案 |



\## 13.3 データモデル



\### 13.3.1 TrackType の拡張



```cpp

// src/core/Track.h

enum class TrackType { Video, Audio, Subtitle, AiGenerated, Storyboard, Unknown };

```



`Storyboard` (JSON `"storyboard"`) を追加する。あわせて `Unknown` を追加する

(理由は \[13.10.4](#13104-スキーマバージョンとマイグレーション))。



`Track` に以下を追加する。



```cpp

class Track

{

public:

&#x20;   /// 映像合成に参加するか。Video / Subtitle / AiGenerated(映像系) が true。

&#x20;   /// Storyboard / Audio / Unknown は false。

&#x20;   bool participatesInComposite() const;



&#x20;   /// オーディオグラフに参加するか。

&#x20;   bool participatesInAudioGraph() const;



&#x20;   // --- AIトラックとの関係 ---

&#x20;   /// このトラックが担う出力役割。空文字なら手動作成の通常トラック。

&#x20;   /// "mainVideo" / "mainVideoB" / "overlay" / "narration" / "bgm" / "se" / "subtitle" / "mask"

&#x20;   QString aiRole() const;

&#x20;   void    setAiRole(const QString\& r);



&#x20;   /// このトラックを生成した絵コンテトラックの ID。null なら無関係。

&#x20;   QUuid   storyboardTrackId() const;

&#x20;   void    setStoryboardTrackId(const QUuid\& id);



&#x20;   /// 役割ごとの既定パラメータ (カスケード第 2 段)。

&#x20;   const QJsonObject\& roleDefaultsPatch() const;



&#x20;   /// ★ 新規: 未知フィールドの保持 (Clip と同じ前方互換の仕組み)

&#x20;   const QJsonObject\& unknownFields() const;

&#x20;   void setUnknownFields(const QJsonObject\& o);

};

```



`acceptsClip()` の規則:



| TrackType | 受け入れる Clip |

|---|---|

| `Storyboard` | \*\*`CutClip` のみ\*\* |

| その他 | `CutClip` を\*\*受け入れない\*\* |



これは全トラック型の中で唯一「排他的」な受け入れ規則である。理由:

カットはメディアではなく\*\*仕様\*\*であり、同じトラックにメディアが混ざると

「AIトラックが真実である」という前提が成り立たなくなる。



> \*\*絵コンテトラックは複数あってよい。\*\* A/B 案の比較、シーン単位のボード分割などに使う。

> 出力の解決は `storyboardTrackId` でスコープされるため、

> 別の絵コンテトラックが同じ出力トラックを取り合うことはない。



\### 13.3.2 CutClip



```cpp

// src/core/Clip.h

enum class ClipType { Video, Audio, Subtitle, AiPlaceholder, Image, Color, Cut };

```



> \*\*命名について\*\*: 域内用語は「カット」なので `CutClip` とする。

> クリップボードの "cut" と紛らわしいため、\*\*コーディング規約として

> `EditController` のクリップボード操作は `copySelection` / `removeSelection` / `paste`

> と命名し、`cut` という名前のメソッドを作らない\*\*。



```cpp

// src/core/CutClip.h

namespace yave {



enum class CutStatus      { NotStarted, Rough, InReview, Approved };

enum class ContinuityMode { None, FromBoardImage, FromPreviousEnd, FromCutId };



enum class ShotSize    { Unspecified, ExtremeWide, Wide, Full, Medium, CloseUp, ExtremeCloseUp };

enum class CameraAngle { Unspecified, EyeLevel, High, Low, BirdsEye, WormsEye, Dutch };

enum class CameraMovement { Unspecified, Fixed, PanLeft, PanRight, TiltUp, TiltDown,

&#x20;                           Dolly, SlowPushIn, PullOut, Handheld, Crane, Follow };

enum class TransitionKind { Cut, Dissolve, FadeToBlack, FadeFromBlack, Wipe, MatchCut };



struct CameraWork

{

&#x20;   ShotSize       size     = ShotSize::Unspecified;

&#x20;   CameraAngle    angle    = CameraAngle::Unspecified;

&#x20;   CameraMovement movement = CameraMovement::Unspecified;

&#x20;   QString        note;                 // 自由記述 ("手前の柵越しに")

};



/// 絵コンテの「画」。

struct BoardImage

{

&#x20;   enum class Origin { None, UserFile, Generated, TimelineFrame };

&#x20;   Origin  origin = Origin::None;

&#x20;   QUuid   assetId;            // UserFile / Generated: AssetLibrary 登録済み

&#x20;   QUuid   sourceTrackId;      // TimelineFrame

&#x20;   int64\_t sourceFrame = 0;    // 〃

&#x20;   QUuid   generatedByTaskId;  // Generated (T2I) の由来タスク

};



/// 前後カットとの繋がり方。

struct Continuity

{

&#x20;   ContinuityMode mode       = ContinuityMode::FromBoardImage;

&#x20;   QUuid          fromCutId;             // FromCutId のときのみ

&#x20;   double         strength   = 0.9;      // ImageReference::strength へ写す

&#x20;   bool           sceneBreak = false;    // true でチェーンを切る (並列化の単位)

};



/// AIトラック上の 1 区間 = 1 カットの演出指示。

class CutClip : public Clip

{

public:

&#x20;   ClipType type() const override { return ClipType::Cut; }

&#x20;   std::shared\_ptr<Clip> clone() const override;



&#x20;   // ---- 人間向けの仕様。これが真実である ----

&#x20;   QString slug()        const;   // 見出し「屋上・朝」

&#x20;   QString label()       const;   // 手動採番 "12b"。既定は空 (自動採番を使う)

&#x20;   QString description() const;   // ト書き。何が起きるか

&#x20;   QString dialogue()    const;   // セリフ / ナレーション原稿

&#x20;   QString mood()        const;   // 「静か / 寂しい」



&#x20;   CameraWork camera() const;

&#x20;   const std::vector<QUuid>\& characterIds() const;   // StoryBible への参照

&#x20;   QUuid   locationId() const;



&#x20;   TransitionKind transitionIn()  const;

&#x20;   TransitionKind transitionOut() const;



&#x20;   BoardImage board() const;



&#x20;   // ---- 連続性 / 承認 ----

&#x20;   Continuity continuity() const;

&#x20;   CutStatus  status()     const;

&#x20;   QString    reviewNote() const;



&#x20;   // ---- 生成 ----

&#x20;   const std::vector<OutputBinding>\& outputs() const;

&#x20;   OutputBinding\*       findOutput(const QUuid\& bindingId);

&#x20;   const OutputBinding\* findOutput(OutputRole role, const QString\& tag = {}) const;



&#x20;   /// カット段のカスケード層 (疎パッチ)。13.3.5 参照

&#x20;   const QJsonObject\& paramPatch() const;

&#x20;   /// artStyle / negativePrompt などをこのカットだけ上書きする

&#x20;   const QJsonObject\& biblePatch() const;



&#x20;   // ---- 表示 ----

&#x20;   /// アニマティック専用。合成には参加しない。13.8 参照

&#x20;   LayerItem makeLayerItem(int64\_t frame, int zIndex, const Track\& t) const override;



&#x20;   /// 未生成カットのキャプション表示に使う一時 SubtitleClip。所有はこのクリップ。

&#x20;   const subtitle::SubtitleClip\* animaticCaption() const;



&#x20;   /// 指定役割の生成仕様ハッシュ。13.6.2 参照。

&#x20;   /// status / reviewNote / label / slug / resolvedTrackId / committed\* / state は含めない。

&#x20;   QByteArray specHash(OutputRole role, const QString\& tag = {}) const;

};



} // namespace yave

```



\#### カット番号を保持しない



カット番号は `Track::clips\_` 内のインデックスから導出する。フィールドとして持たない。



> \*\*理由\*\*: \[3.2.1](03-timeline-render.md) が `zOrder` フィールドを拒否したのと同じ論理である。

> 番号を実体として持つと、挿入・削除・並び替えのたびに再採番と同期が必要になり、

> ずれたときに「表示は 12 番だが内部は 11 番」という追跡困難なバグになる。

> 配列順序を唯一の真実にすれば、この破綻が構造的に起きない。



手動で「12b」のような番号を振りたい要求のために `label` を別に持つ。

`label` が空なら UI は導出番号を表示する。



\### 13.3.3 OutputBinding — 「必要なトラックを生成する」の実体



1 カットは 0 個以上の `OutputBinding` を持ち、\*\*1 バインディング = 1 生成タスク = 1 出力\*\*

である。これが「AI にリクエストすると必要なトラックが生える」挙動の担い手になる。



```cpp

// src/core/CutClip.h

namespace yave {



enum class OutputRole {

&#x20;   MainVideo,     // 主映像

&#x20;   MainVideoB,    // トランジション用の B ロール (13.14.7)

&#x20;   Overlay,       // 前景オーバーレイ

&#x20;   Narration,     // ナレーション (TTS)

&#x20;   Bgm,

&#x20;   SoundEffect,

&#x20;   Subtitle,

&#x20;   Mask

};



enum class TrackResolveMode { Auto, Existing, AlwaysNew };



enum class OutputState {

&#x20;   NotGenerated,  // 未生成

&#x20;   Queued, Running, Cached,

&#x20;   Committed,     // タイムラインに反映済み

&#x20;   Failed,

&#x20;   Stale,         // コミット済みだが仕様が変わった

&#x20;   Blocked        // 上流カットが失敗したため生成しない

};



struct OutputBinding

{

&#x20;   QUuid       id;                     // 安定 ID。Undo と再生成をまたいで維持する

&#x20;   OutputRole  role    = OutputRole::MainVideo;

&#x20;   QString     roleTag;                // 同一役割の複数出力を区別する "se.door"

&#x20;   bool        enabled = true;



&#x20;   // --- 配置先の解決 ---

&#x20;   TrackResolveMode resolveMode = TrackResolveMode::Auto;

&#x20;   QUuid       resolvedTrackId;        // 解決後に書き戻される

&#x20;   QString     trackNameHint;



&#x20;   // --- 出力間の依存 ---

&#x20;   QUuid       derivedFromBindingId;   // 例: subtitle は narration の出力から STT



&#x20;   // --- 尺 ---

&#x20;   int64\_t     leadInFrames  = 0;      // カット区間に対する前伸ばし

&#x20;   int64\_t     leadOutFrames = 0;      // トランジション用の後伸ばし



&#x20;   // --- 仕様 (カスケード最終段) ---

&#x20;   QJsonObject paramPatch;

&#x20;   PromptLock  promptLock;             // 13.4.3



&#x20;   // --- 結果 ---

&#x20;   QUuid       lastTaskId;

&#x20;   std::vector<QUuid> committedClipIds;   // 字幕は複数キューになりうる

&#x20;   QByteArray  committedSpecHash;         // コミット時点の自分の仕様

&#x20;   QByteArray  committedUpstreamHash;     // コミット時点の上流の仕様

&#x20;   OutputState state = OutputState::NotGenerated;

};



} // namespace yave

```



\#### 役割 → トラック型の対応



| OutputRole | TrackType | 生成される Clip |

|---|---|---|

| `MainVideo` / `MainVideoB` / `Overlay` | `Video` | `VideoClip` |

| `Mask` | `Video` (AlphaMask ブレンド) | `VideoClip` |

| `Narration` / `Bgm` / `SoundEffect` | `Audio` | `AudioClip` |

| `Subtitle` | `Subtitle` | `SubtitleClip` 群 |



\#### 解決アルゴリズム (`RoleTrackResolver`)



```cpp

// src/ai/RoleTrackResolver.h

class RoleTrackResolver

{

public:

&#x20;   struct Resolution {

&#x20;       QUuid trackId;

&#x20;       bool  needsCreation = false;

&#x20;       int   insertIndex   = -1;      // needsCreation のとき

&#x20;       TrackType trackType = TrackType::Video;

&#x20;       QString   trackName;

&#x20;   };



&#x20;   /// 副作用なし。Undo コマンドが実際の生成を行う。

&#x20;   Resolution resolve(const Timeline\& tl,

&#x20;                      const Track\& storyboardTrack,

&#x20;                      const CutClip\& cut,

&#x20;                      const OutputBinding\& b) const;

};

```



判定順序:



1\. `b.resolvedTrackId` が非 null かつトラックが実在し型が互換 → \*\*それを使う\*\*

2\. `resolveMode == Existing` で 1 が満たされない → `error.ai.cut.boundTrackMissing`

3\. `resolveMode == Auto` → `aiRole == roleKey(role, roleTag)` かつ

&#x20;  `storyboardTrackId == storyboardTrack.id()` のトラックを検索。最初の一致を使う

4\. 見つからない、または `AlwaysNew` → 新規作成



新規作成時の Z 配置は\*\*役割の優先度表で決定的に\*\*決める。



| 挿入位置 | 役割 |

|---|---|

| 絵コンテトラックの直下から順に | `Mask`, `MainVideo`, `MainVideoB`, `Overlay` |

| その上 | `Subtitle` |

| トラック列の末尾 | `Narration`, `SoundEffect`, `Bgm` (音声は Z 順に無関係) |



これにより、同じプランから 2 回作っても同じ Z 順になる。



> \*\*冪等性が最重要\*\*: `resolvedTrackId` の書き戻しは、トラックを作った

> `AddTrackCommand` を\*\*子コマンドに持つ同一の Undo ステップ\*\*内で行う

> (\[13.9](#139-undo-コマンド))。分けると、undo でトラックだけ消えて

> `resolvedTrackId` が宙を指す状態が到達可能になる。

> また 1 が最初に来ることで、バッチを 2 回流してもトラックは増殖しない。



\### 13.3.4 StoryBible



```cpp

// src/core/StoryBible.h

namespace yave {



struct StoryBibleCharacter

{

&#x20;   QUuid   id;                  // 参照用。design.md §3.3 に従い UUID

&#x20;   QString key;                 // 人間可読キー "aoi"。プロンプト内参照とモデル出力の照合用

&#x20;   QString name;                // 表示名「葵」

&#x20;   QString appearance;          // 「黒髪ショート、紺のセーラー服」

&#x20;   QString personality;

&#x20;   QString promptFragment;      // モデルへ渡す英語断片

&#x20;   QString voiceId;             // TTS の既定話者

&#x20;   ai::ImageReference referenceImage;

};



struct StoryBibleLocation

{

&#x20;   QUuid   id;

&#x20;   QString key;                 // "rooftop"

&#x20;   QString name;

&#x20;   QString description;

&#x20;   QString promptFragment;

&#x20;   ai::ImageReference referenceImage;

};



/// プロジェクト単位の作品設定。Project が所有する。

struct StoryBible

{

&#x20;   QString artStyle;            // 共通画風

&#x20;   QString negativePrompt;      // 全カット共通のネガティブ

&#x20;   QString promptPrefix;

&#x20;   QString promptSuffix;



&#x20;   std::vector<StoryBibleCharacter> characters;

&#x20;   std::vector<StoryBibleLocation>  locations;



&#x20;   QJsonObject roleDefaults;    // role -> 疎パッチ (カスケード第 1 段)

&#x20;   QJsonObject promptTemplates; // role -> テンプレート文字列 (13.4)



&#x20;   QJsonObject unknownFields;



&#x20;   const StoryBibleCharacter\* characterById(const QUuid\&) const;

&#x20;   const StoryBibleCharacter\* characterByKey(const QString\&) const;

&#x20;   const StoryBibleLocation\*  locationById(const QUuid\&) const;

&#x20;   const StoryBibleLocation\*  locationByKey(const QString\&) const;

};



} // namespace yave

```



> \*\*`id` と `key` を両方持つ理由\*\*: `design.md` §3.3 は「ID は UUID、配列インデックスを

> 永続 ID にしない」と定めており、カットからの参照は `QUuid id` で行う。

> 一方 L1 の LLM は UUID を発明できず、プロンプト内でも `{{characters}}` として

> 人間可読な名前で扱いたい。そこで\*\*参照は UUID、照合と表示は `key`\*\* に分ける。

> `key` は `\[a-z0-9.\_-]{1,64}` で検証し、インポート時に一意化する。

> `key` を変えても既存カットの参照は壊れない。



\### 13.3.5 カスケード — Story Bible → トラック → カット → バインディング



生成に使う `ai::AiGenerationParams` は、4 段のマージで組み立てる。



```

&#x20; 役割ごとのベースライン (組み込み既定)

&#x20;       ↓ merge

&#x20; StoryBible::roleDefaults\[role]

&#x20;       ↓ merge

&#x20; Track::roleDefaultsPatch()\[role]        (絵コンテトラック単位の既定)

&#x20;       ↓ merge

&#x20; CutClip::paramPatch()                   (カット単位)

&#x20;       ↓ merge

&#x20; OutputBinding::paramPatch               (出力単位)

&#x20;       ↓

&#x20; 解決済み AiGenerationParams + provenance

```



```cpp

// src/ai/ParamCascade.h

class ParamCascade

{

public:

&#x20;   struct Resolved {

&#x20;       ai::AiGenerationParams params;

&#x20;       QJsonObject provenance;   // キー -> 出所 ("bible.roleDefaults" / "cut" / "binding")

&#x20;   };



&#x20;   Resolved resolve(const StoryBible\& bible,

&#x20;                    const Track\& storyboardTrack,

&#x20;                    const CutClip\& cut,

&#x20;                    const OutputBinding\& binding) const;

};

```



\#### なぜ `std::optional` の列ではなく疎な JSON パッチなのか



`AiGenerationParams` は値初期化されたフラットな構造体である。

そこに値を直接置くと \*\*「ユーザーが意図して `guidanceScale = 7.5` にした」と

「たまたま既定値が 7.5 だった」を区別できない\*\*。

区別できなければ「継承に戻す」ボタンも「上書き中」インジケータも実装できない。



疎パッチなら:



\- \*\*キーの存在そのものが「上書き済み」の定義\*\*になる

\- 「継承に戻す」= `patch.remove(key)`

\- 未知キーが素通りするので、`Clip::unknownFields\_` と同じ理屈で前方互換になる



代償は文字列キーであること。緩和策を規約として定める。



1\. キー名は必ず `io::keys` の定数を使い、リテラルを書かない

2\. `tst\_cutcascade.cpp` が「`AiGenerationParams::toJson()` が出す全キーは

&#x20;  マージ経路を通過して往復する」ことを表明する



\#### どの段が何を持つか



| フィールド群 | 既定の所在 | 上書き可能な段 |

|---|---|---|

| `artStyle` / `negativePrompt` / prefix / suffix | Story Bible | カット (`biblePatch`) |

| `modelId` / `providerId` / `steps` / `guidanceScale` / `outputResolution` / `outputFrameRate` | Bible `roleDefaults` | トラック → カット → バインディング |

| `voiceId` | キャラクター定義 → Bible `roleDefaults` | カット → バインディング |

| `speakingRate` / `pitch` / `targetLufs` | Bible `roleDefaults` | カット → バインディング |

| `seed` | 常に `-1` (\*\*継承しない\*\*) | バインディングのみ |

| `range` | \*\*継承不可\*\*。`cut.range()` と `leadIn/OutFrames` から必ず導出する | なし |

| `targetTrackId` / `createNewTrack` / `replaceExistingClips` | \*\*継承不可\*\*。`OutputBinding` が唯一の真実 | なし |



> `seed` を継承させない理由: 継承すると Story Bible に seed を書いた瞬間、

> 全カットが同じ seed で生成され、意図せず似た画になる。

> 再現性が要るのは「このカットをもう一度」であって「全カットを同じ乱数で」ではない。



\## 13.4 プロンプト合成



\### 13.4.1 CutPromptComposer



カットの構造化フィールドは\*\*人間のための表現\*\*であり、モデルへ渡す文字列とは別物である。

両者を分けたまま、決定的に片方からもう片方を作る。



```cpp

// src/ai/CutPromptComposer.h

namespace yave::ai {



struct ComposedPrompt

{

&#x20;   QString     prompt;

&#x20;   QString     negativePrompt;

&#x20;   QJsonObject provenance;    // 断片 -> 出所 ("bible.artStyle" / "cut.description" / …)

&#x20;   QByteArray  sourceHash;    // 合成元となった仕様のハッシュ

&#x20;   QStringList warnings;      // 未解決プレースホルダ等

&#x20;   bool        fromLock = false;

};



class CutPromptComposer

{

public:

&#x20;   ComposedPrompt compose(const StoryBible\& bible,

&#x20;                          const CutClip\& cut,

&#x20;                          const OutputBinding\& binding) const;



&#x20;   /// 組み込み既定テンプレート (:/ai/prompt\_templates.json)

&#x20;   static QString defaultTemplate(OutputRole role);

};



} // namespace yave::ai

```



\### 13.4.2 テンプレート



役割ごとのテンプレートは `StoryBible::promptTemplates` に\*\*永続化する\*\*。

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



未知のプレースホルダは\*\*空に展開し警告を積む\*\*。テンプレートはユーザーが編集する

対象なので、例外を投げてはならない。



\### 13.4.3 プロンプトは翻訳経路に載せない



`CameraMovement::SlowPushIn` → `"slow push-in"` のような\*\*モデル向けフレーズ表は

UI 翻訳とは完全に別系統\*\*にする。



> \*\*規約\*\*: プロンプト文字列を `tr()` / `qsTr()` に通してはならない。

> UI 言語を日本語にしただけでモデルへ渡る語彙が変わると、同じプロジェクトが

> 環境によって違う絵を出す。UI ラベルは `tr()`、プロンプト断片は

> `CameraPhraseTable` (英語固定、`:/ai/camera\_phrases.json`) から取る。

> \[10 章](10-i18n.md) にもこの規約を追記する。



\### 13.4.4 手編集とロック



```cpp

struct PromptLock

{

&#x20;   bool       locked = false;

&#x20;   QString    prompt;

&#x20;   QString    negativePrompt;

&#x20;   QByteArray lockedAgainstHash;   // ロックした時点の ComposedPrompt::sourceHash

};

```



\- カットインスペクタは合成後のプロンプトを\*\*読み取り専用\*\*で表示し、

&#x20; provenance ガター (どのフィールド由来かの帯) を添える。断片をクリックすると

&#x20; 該当フィールドへジャンプする。

\- 「編集 / ロック」を押すと合成結果が `PromptLock` にコピーされ、以後は

&#x20; ロック文字列が使われる。

\- ロック後に仕様が変わると `compose().sourceHash != lockedAgainstHash` になり、

&#x20; 「仕様が変更されています」バッジと「再合成」「差分を見る」を出す。

&#x20; \*\*黙って再合成もしないし、黙って古いまま使うこともしない。\*\*

\- ロックは\*\*カット単位ではなく `OutputBinding` 単位\*\*。映像のプロンプトと

&#x20; ナレーション原稿は別物であり、片方をロックしても他方は追従してよい。

\- `specHash` はロック中ならロック文字列を含む。よってロック・編集は正しく

&#x20; 「再生成が必要」と判定される。



\## 13.5 選択モデル



「選択した区間に対して生成する」には選択の定義が要るが、\*\*既存 12 章のどこにも

選択モデルが定義されていない\*\*。\[6.9.2](06-subtitle-engine.md) の

`addEffectToSelectedSubtitles()` の時点で既に穴が空いている。本節で最小限を埋める。



```cpp

// src/app/models/SelectionModel.h

namespace yave::app {



enum class SelectionMode { Replace, Add, Toggle, ExtendRange };



struct TimelineSelection

{

&#x20;   std::vector<QUuid>       clipIds;

&#x20;   std::vector<QUuid>       trackIds;

&#x20;   std::optional<TimeRange> range;

&#x20;   std::vector<QUuid>       rangeTrackIds;   // 空 = 全トラックに掛かる範囲選択

&#x20;   QUuid                    primaryClipId;   // インスペクタが表示する主対象

};



class SelectionModel : public QObject

{

&#x20;   Q\_OBJECT

&#x20;   Q\_PROPERTY(int  clipCount READ clipCount NOTIFY selectionChanged)

&#x20;   Q\_PROPERTY(bool hasRange  READ hasRange  NOTIFY selectionChanged)

public:

&#x20;   Q\_INVOKABLE void selectClip(const QUuid\& id, SelectionMode m);

&#x20;   Q\_INVOKABLE void selectTrack(const QUuid\& id, SelectionMode m);

&#x20;   Q\_INVOKABLE void setRange(qint64 start, qint64 duration);

&#x20;   Q\_INVOKABLE void clear();



&#x20;   const TimelineSelection\& selection() const;



&#x20;   /// 生成対象カットの解決:

&#x20;   ///   明示選択されたカット ∪ (range に交差する可視 Storyboard トラック上のカット)

&#x20;   std::vector<QUuid> resolvedCutIds(const Timeline\& tl) const;



signals:

&#x20;   void selectionChanged();

};



} // namespace yave::app

```



規則:



\- \*\*Undo の対象にしない。プロジェクト JSON にも保存しない。\*\*

&#x20; 永続するカーソル状態は既存の `playhead` と `workRange` だけである。

\- ID のみを保持する。`Timeline::clipRemoved` / `trackRemoved` を購読して自己修復する。

\- カット解決は「明示選択が優先。空なら範囲に\*\*交差\*\*するカット」。

&#x20; 部分的にしか掛かっていないカットも含めるが、確認ダイアログにその旨を書く。

\- `EditController` が所有し、QML へコンテキストプロパティとして公開する。

&#x20; `StoryboardController` は読むだけ。



> \*\*ボードビューとタイムラインの双方向同期は、両者が同じ `SelectionModel` に

> bind することで自動的に満たされる。\*\* 個別に同期コードを書かない。

YAVEEOF

wc -l doc/design/13-ai-track.md



Exit code 2

/usr/bin/bash: -c: line 1: unexpected EOF while looking for matching `''

