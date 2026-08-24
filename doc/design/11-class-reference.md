# 11. クラスリファレンス (主要クラスの .h 定義)

[← 目次に戻る](../design.md)

---

本章は、そのままヘッダファイルとして使える粒度でクラス定義を示す。
インクルードガードは `#pragma once` に統一する。

---

## 11.1 core/Clip.h

```cpp
#pragma once

#include "TimeRange.h"
#include "BlendMode.h"

#include <QUuid>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QRectF>

#include <memory>

namespace yave {

class Track;
struct LayerItem;

enum class ClipType { Video, Audio, Subtitle, AiPlaceholder, Image, Color };

/// タイムライン上に置かれる編集単位の基底クラス。
/// 派生: VideoClip / AudioClip / SubtitleClip / AiPlaceholderClip
class Clip
{
public:
    virtual ~Clip() = default;

    // --- 同一性 ---
    QUuid id() const { return id_; }
    void  setId(const QUuid& id) { id_ = id; }

    virtual ClipType type() const = 0;

    /// 深いコピー。新しい id を振る。分割 / 複製で使う。
    virtual std::shared_ptr<Clip> clone() const = 0;

    // --- 時間 ---
    const TimeRange& range() const { return range_; }
    void setRange(const TimeRange& r) { range_ = r; }

    /// ソース内の開始オフセット (フレーム)。字幕やカラークリップでは 0 固定。
    virtual int64_t sourceOffset() const { return 0; }
    virtual void    setSourceOffset(int64_t) {}
    virtual void    shiftSourceOffset(int64_t delta) { setSourceOffset(sourceOffset() + delta); }

    /// トリム可能な最大長 (ソースの尺)。無制限なら -1。
    virtual int64_t maxDuration() const { return -1; }

    // --- 表示 ---
    QString  name() const { return name_; }
    void     setName(const QString& n) { name_ = n; }

    bool     isEnabled() const { return enabled_; }
    void     setEnabled(bool e) { enabled_ = e; }

    bool     isLocked() const { return locked_; }
    void     setLocked(bool l) { locked_ = l; }

    // --- 合成 ---
    double    opacity() const { return opacity_; }
    void      setOpacity(double o) { opacity_ = qBound(0.0, o, 1.0); }

    BlendMode blendMode() const { return blendMode_; }
    void      setBlendMode(BlendMode m) { blendMode_ = m; }

    const QMatrix4x4& transform() const { return transform_; }
    void  setTransform(const QMatrix4x4& m) { transform_ = m; }

    const QRectF& cropRect() const { return crop_; }
    void  setCropRect(const QRectF& r) { crop_ = r; }

    // --- フェード ---
    int64_t fadeInFrames()  const { return fadeIn_; }
    int64_t fadeOutFrames() const { return fadeOut_; }
    void    setFadeInFrames(int64_t f)  { fadeIn_ = std::max<int64_t>(0, f); }
    void    setFadeOutFrames(int64_t f) { fadeOut_ = std::max<int64_t>(0, f); }

    /// フェードを加味した最終不透明度
    double effectiveOpacity(int64_t frame) const;

    // --- AI 由来の記録 ---
    QUuid generatedByTaskId() const { return generatedByTaskId_; }
    void  setGeneratedByTaskId(const QUuid& id) { generatedByTaskId_ = id; }
    bool  isAiGenerated() const { return !generatedByTaskId_.isNull(); }

    // --- レンダリング ---
    /// RenderSnapshot に載せる 1 レイヤー分の情報を作る。
    /// frame はタイムライン絶対フレーム。
    virtual LayerItem makeLayerItem(int64_t frame, int zIndex, const Track& track) const = 0;

    // --- 永続化 (未知フィールドの保持) ---
    const QJsonObject& unknownFields() const { return unknownFields_; }
    void setUnknownFields(const QJsonObject& o) { unknownFields_ = o; }

protected:
    Clip() : id_(QUuid::createUuid()) {}
    Clip(const Clip&) = default;

    /// clone() の実装で共通部分をコピーするヘルパ
    void copyBaseTo(Clip& dst) const;

private:
    QUuid       id_;
    QString     name_;
    TimeRange   range_;
    bool        enabled_   = true;
    bool        locked_    = false;
    double      opacity_   = 1.0;
    BlendMode   blendMode_ = BlendMode::Normal;
    QMatrix4x4  transform_;
    QRectF      crop_{0.0, 0.0, 1.0, 1.0};
    int64_t     fadeIn_    = 0;
    int64_t     fadeOut_   = 0;
    QUuid       generatedByTaskId_;
    QJsonObject unknownFields_;
};

} // namespace yave
```

---

## 11.2 core/Track.h

```cpp
#pragma once

#include "Clip.h"
#include "TimeRange.h"

#include <QUuid>
#include <QString>
#include <QColor>

#include <memory>
#include <vector>
#include <functional>

namespace yave {

enum class TrackType { Video, Audio, Subtitle, AiGenerated };

namespace plugin { class Vst3Host; }

/// タイムライン上の 1 レイヤー。
/// 不変条件:
///   (1) clips_ は range().start の昇順にソートされている
///   (2) clips_ の各要素の TimeRange は互いに重ならない
///   (3) すべての clip の duration > 0
class Track
{
public:
    explicit Track(TrackType type);
    ~Track();

    Track(const Track&) = delete;
    Track& operator=(const Track&) = delete;

    // --- 同一性 / 属性 ---
    QUuid     id() const { return id_; }
    void      setId(const QUuid& id) { id_ = id; }

    TrackType type() const { return type_; }

    QString   name() const { return name_; }
    void      setName(const QString& n) { name_ = n; }

    QColor    color() const { return color_; }
    void      setColor(const QColor& c) { color_ = c; }

    int       uiHeight() const { return uiHeight_; }
    void      setUiHeight(int h) { uiHeight_ = qBound(24, h, 400); }

    // --- 状態 ---
    bool isVisible() const { return visible_; }
    void setVisible(bool v) { visible_ = v; }

    bool isLocked() const { return locked_; }
    void setLocked(bool l) { locked_ = l; }

    bool isMuted() const { return muted_; }
    void setMuted(bool m) { muted_ = m; }

    bool isSolo() const { return solo_; }
    void setSolo(bool s) { solo_ = s; }

    // --- 映像トラック用 ---
    double    opacity() const { return opacity_; }
    void      setOpacity(double o) { opacity_ = qBound(0.0, o, 1.0); }
    BlendMode blendMode() const { return blendMode_; }
    void      setBlendMode(BlendMode m) { blendMode_ = m; }

    // --- 音声トラック用 ---
    double gain() const { return gain_; }
    void   setGain(double g) { gain_ = g; }
    double pan() const { return pan_; }
    void   setPan(double p) { pan_ = qBound(-1.0, p, 1.0); }

    // --- 字幕トラック用 ---
    QString defaultStylePresetId() const { return defaultStylePresetId_; }
    void    setDefaultStylePresetId(const QString& id) { defaultStylePresetId_ = id; }

    // ================= クリップ操作 =================

    /// 受け入れ可能なクリップ型かどうか。UI のドロップ判定に使う。
    bool acceptsClip(const Clip& c) const;

    /// ソート順を保って挿入する。
    /// 既存クリップと重なる場合は false を返し、挿入しない。
    bool insertClip(const std::shared_ptr<Clip>& c);

    /// 重なりを許して強制挿入 (重なる部分の既存クリップをトリム / 分割する)。
    /// 上書き編集モードで使う。
    void overwriteClip(const std::shared_ptr<Clip>& c);

    /// 除去して返す。見つからなければ nullptr。
    std::shared_ptr<Clip> takeClip(const QUuid& clipId);
    bool                  removeClip(const QUuid& clipId);

    /// 指定範囲に完全に含まれるクリップを除去する。
    /// 部分的に掛かるクリップはトリムされる。
    std::vector<std::shared_ptr<Clip>> removeClipsIn(const TimeRange& r);

    /// startFrame 以降のクリップを delta フレームだけずらす (リップル編集)
    void shiftClipsAfter(int64_t startFrame, int64_t delta);

    // --- 検索 ---
    std::shared_ptr<Clip> clipAt(int64_t frame) const;                 // O(log n)
    std::shared_ptr<Clip> clipById(const QUuid& id) const;             // O(n)
    std::vector<std::shared_ptr<Clip>> clipsIn(const TimeRange& r) const;  // O(log n + k)

    /// frame より後ろにある最初のクリップの開始フレーム。無ければ -1。
    int64_t nextClipStart(int64_t frame) const;
    int64_t prevClipEnd(int64_t frame) const;

    const std::vector<std::shared_ptr<Clip>>& clips() const { return clips_; }
    size_t  clipCount() const { return clips_.size(); }
    int64_t contentDuration() const;    // 最後のクリップの end()

    /// クリップの範囲が外部で変更された後に呼び、ソート順を復元する。
    void resort();

    // --- エフェクトチェーン (音声トラックの VST3) ---
    const std::vector<plugin::Vst3Host*>& effectChain() const { return effectChain_; }
    void   addEffect(plugin::Vst3Host* fx);
    void   removeEffect(int index);
    void   moveEffect(int from, int to);
    int64_t totalLatencySamples() const;

    // --- デバッグ ---
    void assertInvariants() const;      // Debug ビルドのみ実体を持つ

private:
    /// 挿入位置 (start 昇順を保つ位置) を返す
    std::vector<std::shared_ptr<Clip>>::iterator lowerBoundFor(int64_t start);
    std::vector<std::shared_ptr<Clip>>::const_iterator lowerBoundFor(int64_t start) const;

    QUuid      id_;
    TrackType  type_;
    QString    name_;
    QColor     color_{0x3a, 0x5f, 0x8a};
    int        uiHeight_ = 64;

    bool       visible_ = true;
    bool       locked_  = false;
    bool       muted_   = false;
    bool       solo_    = false;

    double     opacity_   = 1.0;
    BlendMode  blendMode_ = BlendMode::Normal;
    double     gain_      = 1.0;
    double     pan_       = 0.0;

    QString    defaultStylePresetId_ = QStringLiteral("default");

    std::vector<std::shared_ptr<Clip>> clips_;      // start 昇順
    std::vector<plugin::Vst3Host*>     effectChain_; // 所有はしない
};

} // namespace yave
```

---

## 11.3 core/Timeline.h

```cpp
#pragma once

#include "Track.h"
#include "Rational.h"
#include "RenderSnapshot.h"

#include <QObject>
#include <QSize>
#include <QUuid>

#include <memory>
#include <vector>

namespace yave {

class Project;

/// 無限レイヤー (トラック数無制限) を保持するコンテナ。
///
/// tracks_ のインデックスがそのまま Z オーダーになる。
///   index 0        = 最背面
///   index size()-1 = 最前面
/// 別途 zOrder フィールドは持たない (二重管理を避けるため)。
///
/// スレッド安全性:
///   構造を変更してよいのは UI スレッドのみ。
///   Render / Audio スレッドは buildSnapshot() / buildAudioGraph() の
///   結果 (値のコピー) 越しにのみ参照する。
class Timeline : public QObject
{
    Q_OBJECT
public:
    explicit Timeline(Project* project, QObject* parent = nullptr);
    ~Timeline() override;

    Project* project() const { return project_; }

    // ================= トラック操作 =================

    /// 末尾 (最前面) に追加する。
    Track* appendTrack(TrackType type, const QString& name = {});

    /// 指定位置に挿入する。index == size() なら末尾。
    Track* insertTrack(int index, TrackType type, const QString& name = {});

    /// 所有権ごと取り出す。Undo コマンドが保持するために使う。
    std::unique_ptr<Track> takeTrack(int index);
    std::unique_ptr<Track> takeTrackById(const QUuid& id);

    /// takeTrack で取り出したトラックを元の位置へ戻す。
    void reinsertTrack(int index, std::unique_ptr<Track> track);

    /// 並び替え。Z オーダーが変わる。
    void moveTrack(int from, int to);

    int    trackCount() const { return int(tracks_.size()); }
    Track* trackAt(int index) const;
    Track* trackById(const QUuid& id) const;
    int    indexOfTrack(const Track* t) const;
    int    indexOfTrack(const QUuid& id) const;

    /// 型でフィルタした一覧
    std::vector<Track*> tracksOfType(TrackType type) const;

    // ================= クリップ横断操作 =================

    /// 全トラックから id で探す
    std::shared_ptr<Clip> findClip(const QUuid& clipId, Track** ownerOut = nullptr) const;

    /// すべてのトラックの最大 end()
    int64_t duration() const;

    /// 指定フレームでのスナップ候補 (全トラックのクリップ境界)
    std::vector<int64_t> snapCandidates(const TimeRange& visibleRange,
                                        const std::vector<int>& visibleTrackIndices) const;

    // ================= AI プレースホルダ =================

    void insertPlaceholder(int trackIndex, const std::shared_ptr<Clip>& placeholder);
    std::shared_ptr<Clip> takePlaceholder(const QUuid& taskId);
    void restorePlaceholder(const std::shared_ptr<Clip>& placeholder);
    void updatePlaceholderProgress(const QUuid& taskId, double progress);

    // ================= レンダリング =================

    Rational timebase() const { return timebase_; }
    void     setTimebase(const Rational& tb);

    QSize    canvasSize() const { return canvasSize_; }
    void     setCanvasSize(const QSize& s);

    /// Render スレッドへ渡すための不変スナップショットを作る。
    /// UI スレッドから毎フレーム呼ばれる。O(トラック数 * log(クリップ数))。
    RenderSnapshot buildSnapshot(int64_t frame) const;

    /// 変更のたびにインクリメントされる。キャッシュ無効化判定に使う。
    uint64_t revision() const { return revision_; }

signals:
    void trackInserted(int index);
    void trackRemoved(int index);
    void trackMoved(int from, int to);
    void trackChanged(int index);
    void clipInserted(const QUuid& trackId, const QUuid& clipId);
    void clipRemoved(const QUuid& trackId, const QUuid& clipId);
    void clipChanged(const QUuid& trackId, const QUuid& clipId);
    void structureChanged();          // 音声グラフの再構築が必要になる変更
    void durationChanged(int64_t newDuration);

private:
    void bumpRevision() { ++revision_; }

    Project*                            project_ = nullptr;
    std::vector<std::unique_ptr<Track>> tracks_;     // index = Z オーダー
    Rational                            timebase_ = timebase::Fps59_94;
    QSize                               canvasSize_{3840, 2160};
    uint64_t                            revision_ = 0;
};

} // namespace yave
```

---

## 11.4 subtitle/SubtitleClip.h

```cpp
#pragma once

#include "../core/Clip.h"
#include "SubtitleText.h"
#include "SubtitleStyle.h"
#include "SubtitleEffectInstance.h"

#include <vector>

namespace yave::subtitle {

/// 字幕区間。SRT の 1 キューがこれ 1 個に対応する。
/// タイムライン上では他のクリップと完全に同等に扱われる
/// (移動 / トリム / 分割 / 複製 / 別トラックへ移動 / リップル編集)。
class SubtitleClip : public Clip
{
public:
    SubtitleClip();
    ~SubtitleClip() override;

    ClipType type() const override { return ClipType::Subtitle; }
    std::shared_ptr<Clip> clone() const override;

    // --- テキスト ---
    const SubtitleText& text() const { return text_; }
    void setText(const SubtitleText& t);

    /// 便利メソッド。リッチスパンは維持されない。
    QString plainText() const { return text_.plain(); }
    void    setPlainText(const QString& s);

    // --- スタイル ---
    /// プロジェクト共通のスタイルプリセットへの参照。
    QString stylePresetId() const { return stylePresetId_; }
    void    setStylePresetId(const QString& id);

    /// プリセットからの差分。これだけがクリップ固有の値。
    const SubtitleStyleDiff& styleOverride() const { return styleOverride_; }
    void  setStyleOverride(const SubtitleStyleDiff& d);
    void  clearStyleOverride();

    /// プリセット + 差分を解決した最終スタイル。
    /// presets は Project が保持するテーブル。
    SubtitleStyle resolvedStyle(const SubtitleStylePresetTable& presets) const;

    // --- エフェクトスタック ---
    /// 下から順に適用される。index 0 が最初。
    const std::vector<SubtitleEffectInstance>& effectStack() const { return effects_; }
    std::vector<SubtitleEffectInstance>&       mutableEffectStack();

    void addEffect(const SubtitleEffectInstance& inst);
    void insertEffect(int index, const SubtitleEffectInstance& inst);
    void removeEffect(int index);
    void moveEffect(int from, int to);
    void setEffectEnabled(int index, bool enabled);

    /// 未インストールプラグインを参照しているエフェクトがあるか
    bool hasMissingEffects() const;

    /// AviUtl アダプタを含むか (含む場合グリフ単位アニメーションと併用不可)
    bool hasBlockLevelEffect() const;

    // --- STT 由来の単語タイミング ---
    struct WordTiming
    {
        int    charStart  = 0;
        int    charLength = 0;
        double startSec   = 0.0;    // クリップ In からの相対秒
        double endSec     = 0.0;
    };
    const std::vector<WordTiming>& wordTimings() const { return wordTimings_; }
    void setWordTimings(std::vector<WordTiming> w) { wordTimings_ = std::move(w); }
    bool hasWordTimings() const { return !wordTimings_.empty(); }

    // --- キャッシュ無効化 ---
    /// テキスト / スタイルが変わるとインクリメントされる。
    /// SubtitleRenderer がレイアウトとグリフアトラスの再生成判定に使う。
    uint64_t contentRevision() const { return contentRevision_; }

    // --- Clip インタフェース ---
    LayerItem makeLayerItem(int64_t frame, int zIndex, const Track& track) const override;

    /// 字幕はソースを持たないので常に 0 / 無制限
    int64_t sourceOffset() const override { return 0; }
    int64_t maxDuration() const override { return -1; }

private:
    void bumpContentRevision() { ++contentRevision_; }

    SubtitleText                        text_;
    QString                             stylePresetId_ = QStringLiteral("default");
    SubtitleStyleDiff                   styleOverride_;
    std::vector<SubtitleEffectInstance> effects_;
    std::vector<WordTiming>             wordTimings_;
    uint64_t                            contentRevision_ = 0;
};

} // namespace yave::subtitle
```

---

## 11.5 sdk/ISubtitleEffect.h

外部プラグイン作者に公開するヘッダ。**ABI 安定性を保つため、
仮想関数の追加は末尾のみ、既存の順序変更は禁止**とする。

```cpp
#pragma once

#include "ParameterSchema.h"
#include "YaveSdkVersion.h"

#include <QMatrix4x4>
#include <QColor>
#include <QSize>
#include <QString>

#include <vector>

class QWidget;

namespace yave::sdk {

/// グリフ 1 個分の、フレームごとに変化する状態。
/// エフェクトはこれだけを書き換える。
struct GlyphTransform
{
    QMatrix4x4 transform;                  // グリフ中心を原点とする変換
    QColor     color      = Qt::white;     // 乗算色
    float      opacity    = 1.0f;
    bool       visible    = true;
    float      blurRadius = 0.0f;          // px
};

/// レイアウト済みグリフの静的情報 (読み取り専用)
struct GlyphInfo
{
    int    charIndex    = 0;
    int    wordIndex    = 0;
    int    lineIndex    = 0;
    QRectF layoutRect;
    QRectF atlasUv;
    QColor baseColor;
    bool   isWhitespace = false;
};

struct SubtitleGlyphRun
{
    std::vector<GlyphInfo> glyphs;
    QSizeF                 blockSize;
    int                    lineCount = 0;
    QSize                  atlasSize;
    quint64                cacheKey  = 0;
};

/// apply() に渡される I/O 構造体
struct SubtitleEffectFrame
{
    // ---- 読み取り専用 ----
    const SubtitleGlyphRun* run = nullptr;
    QSize   canvasSize;
    qint64  clipStartFrame = 0;
    qint64  clipDuration   = 0;
    qint64  currentFrame   = 0;
    double  fps            = 60.0;

    /// STT 由来の単語タイミング。無い場合は空。
    struct WordTiming { int charStart; int charLength; double startSec; double endSec; };
    const std::vector<WordTiming>* wordTimings = nullptr;

    // ---- 読み書き (エフェクトはここだけを変更する) ----
    std::vector<GlyphTransform>* glyphs         = nullptr;   // run->glyphs と同要素数
    QMatrix4x4*                  blockTransform = nullptr;
    float*                       blockOpacity   = nullptr;
};

/// 時間情報
struct SubtitleTimeInfo
{
    double progress        = 0.0;   // 0.0 (In) .. 1.0 (Out)
    double secondsFromIn   = 0.0;
    double secondsToOut    = 0.0;
    double clipDurationSec = 0.0;
};

/// 字幕エフェクトのインタフェース。
/// 組み込みエフェクトも外部プラグインもこれを実装する。
///
/// 契約:
///   - apply() はグリフアトラスの再生成を要求してはならない
///   - apply() は frame.run を変更してはならない
///   - apply() はメモリ確保を避けること (prepare() で確保する)
///   - apply() は同じ入力に対して同じ出力を返すこと (決定的)
///   - apply() はスレッドセーフである必要はない (1 インスタンス = 1 スレッド)
class ISubtitleEffect
{
public:
    virtual ~ISubtitleEffect() = default;

    /// 一意な識別子。プロジェクト JSON に保存される。
    /// 一度公開したら変更してはならない。
    /// 命名: "<vendor>.<effect>" 例 "yave.typewriter" / "com.example.glitch"
    virtual QString id() const = 0;

    /// UI 表示名。翻訳キー ("effect.foo.name") を返してもよい。
    /// 翻訳が見つからなければそのまま表示される。
    virtual QString displayName() const = 0;

    /// カテゴリ ("Transition" / "Motion" / "Color" / "Distort" / "AviUtl")
    virtual QString category() const = 0;

    /// パラメータ定義。UI はこれを見てフォームを自動生成する。
    virtual ParameterSchema parameterSchema() const = 0;

    /// クリップのテキスト / スタイル / 区間が確定したときに 1 回だけ呼ばれる。
    /// 重い前計算 (乱数テーブル、per-glyph の初期値) はここで行う。
    virtual void prepare(const SubtitleGlyphRun& run,
                         const ParameterValues& params,
                         const QSize& canvasSize) { Q_UNUSED(run); Q_UNUSED(params);
                                                    Q_UNUSED(canvasSize); }

    /// 毎フレーム呼ばれる。
    virtual void apply(SubtitleEffectFrame& frame,
                       const SubtitleTimeInfo& time,
                       const ParameterValues& params) = 0;

    /// 独自 GUI を持つか。true なら PluginWindow に埋め込まれる。
    virtual bool     hasCustomEditor() const { return false; }
    virtual QWidget* createEditor(QWidget* parent) { Q_UNUSED(parent); return nullptr; }

    /// このエフェクトがブロック単位で動作するか (グリフ単位アニメーションと併用不可)。
    /// AviUtl アダプタが true を返す。
    virtual bool isBlockLevel() const { return false; }
};

} // namespace yave::sdk
```

---

## 11.6 plugin/PluginManager.h

```cpp
#pragma once

#include "PluginDescriptor.h"

#include <QObject>
#include <QUuid>
#include <QStringList>

#include <vector>

namespace yave::plugin {

class Vst3Registry;
class SubtitleEffectRegistry;
class AviUtlRegistry;          // Windows 以外では定義されない (前方宣言のみ)
class Vst3Host;

/// 字幕エフェクトの所有ポインタ。
/// 外部プラグイン由来のインスタンスは、そのプラグインの destroyEffect で
/// 解放する必要があるため、deleter を型に含める。
using SubtitleEffectPtr =
    std::unique_ptr<yave::sdk::ISubtitleEffect,
                    std::function<void(yave::sdk::ISubtitleEffect*)>>;

class PluginManager : public QObject
{
    Q_OBJECT
public:
    static PluginManager& instance();

    // ================= 走査 =================

    /// バックグラウンドで走査を開始する。起動時に一度呼ぶ。
    /// plugin_cache.json のキャッシュが有効なファイルは再走査しない。
    void scanAsync();
    void scanSync();                    // テスト用
    bool isScanning() const;
    void cancelScan();

    // ================= 一覧 =================

    std::vector<PluginDescriptor> plugins(PluginKind kind) const;
    std::vector<PluginDescriptor> allPlugins() const;

    const PluginDescriptor* find(const QUuid& uid) const;
    const PluginDescriptor* findByNativeId(PluginKind kind, const QString& nativeId) const;

    // ================= 検索パス =================

    QStringList searchPaths(PluginKind kind) const;
    void        setSearchPaths(PluginKind kind, const QStringList& paths);
    void        addSearchPath(PluginKind kind, const QString& path);
    static QStringList defaultSearchPaths(PluginKind kind);

    // ================= インスタンス生成 =================

    /// VST3 プラグインをロードしてホストを返す。所有権は呼び出し側。
    std::unique_ptr<Vst3Host> createVst3(const QUuid& uid,
                                         double sampleRate, int maxBlockSize,
                                         QString* errorOut = nullptr);

    /// 字幕エフェクトのプロトタイプを取得する (一覧表示 / parameterSchema 用)。
    /// 所有権は Registry 側にある。クリップへ積む用途には使わない。
    const yave::sdk::ISubtitleEffect* subtitleEffectPrototype(const QString& effectId) const;

    /// 字幕エフェクトの新しいインスタンスを生成する。
    /// prepare() の前計算結果をインスタンスが保持するため、
    /// クリップごとに個別のインスタンスが必要になる。
    /// 戻り値は呼び出し側が所有する (deleter がプラグインの destroyEffect を呼ぶ)。
    SubtitleEffectPtr createSubtitleEffect(const QString& effectId) const;

    bool isSubtitleEffectMissing(const QString& effectId) const;

    // ================= AviUtl (Windows のみ有効) =================

    /// このビルド / プラットフォームで AviUtl が利用可能か。
    /// macOS では常に false。
    static bool isAviUtlSupported();

    // ================= ブラックリスト =================

    void blacklist(const QString& filePath, const QString& reason);
    void removeFromBlacklist(const QString& filePath);
    void clearBlacklist();
    bool isBlacklisted(const QString& filePath) const;
    std::vector<std::pair<QString, QString>> blacklistEntries() const;

    // ================= 終了処理 =================

    /// シャットダウン時に呼ぶ。オーディオ RT スレッドを止めた後でなければならない。
    void unloadAll();

signals:
    void scanStarted();
    void scanProgress(int done, int total, const QString& currentFile);
    void scanFinished();
    void pluginCrashed(const QString& filePath, const QString& reason);

private:
    PluginManager();
    ~PluginManager() override;

    void loadCache();
    void saveCache() const;

    Vst3Registry*           vst3_       = nullptr;
    SubtitleEffectRegistry* subtitleFx_ = nullptr;
    AviUtlRegistry*         aviutl_     = nullptr;   // 非 Windows では常に nullptr
    mutable QMutex          mutex_;
};

} // namespace yave::plugin
```

---

## 11.7 plugin/PluginWindow.h

```cpp
#pragma once

#include "PluginDescriptor.h"

#include <QWidget>
#include <functional>

namespace yave::plugin {

/// プラグイン GUI を載せるポップアップウィンドウ。
/// VST3 / AviUtl / 字幕エフェクト のすべてで共用する。
///
/// nativeViewHandle() が返す値:
///   Windows -> HWND   (void* にキャスト済み)
///   macOS   -> NSView* (void* にキャスト済み。NSWindow* ではない)
class PluginWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PluginWindow(const PluginDescriptor& desc, QWidget* parent = nullptr);
    ~PluginWindow() override;

    const PluginDescriptor& descriptor() const { return desc_; }

    /// プラグインへ渡すネイティブハンドル。
    /// コンストラクタで winId() を呼んで生成済みなので、常に有効な値を返す。
    void* nativeViewHandle() const;

    /// クライアント領域 (プラグインが描画する範囲) を指定サイズにする。
    /// ウィンドウ枠の分は自動で加算される。
    void resizeClientArea(int width, int height);

    /// IPlugFrame::resizeView から呼ばれる。
    void onPluginRequestedResize(int width, int height);

    /// IPlugView::canResize() の結果を反映する。
    void setResizableByUser(bool on);
    bool isResizableByUser() const { return resizable_; }

    /// ウィンドウを閉じるときに呼ぶ処理 (IPlugView::removed など) を登録する。
    void setDetachHandler(std::function<void()> fn) { detachHandler_ = std::move(fn); }

    /// DPI 非対応プラグイン (AviUtl 等) 用にスケーリングを無効化する。
    /// コンストラクタでネイティブハンドルを作る前に効果を持つため、
    /// PluginDescriptor::kind から自動的に決定される。
    bool isDpiUnaware() const { return dpiUnaware_; }

    /// 既に開いているウィンドウを前面に出す。
    void raiseAndActivate();

#if defined(Q_OS_WIN)
    /// AviUtl フィルタのメッセージ中継先を設定する。
    /// nativeEvent() から func_WndProc へ転送するために使う。
    void setAviUtlMessageTarget(void* filterDll);
#endif

signals:
    void closed();
    void parametersChangedByPlugin();

protected:
    void closeEvent(QCloseEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* e) override;        // LanguageChange
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void retranslateUi();
    void createNativeContainer();

    PluginDescriptor      desc_;
    QWidget*              container_ = nullptr;   // WA_NativeWindow を持つ子
    std::function<void()> detachHandler_;
    bool                  resizable_  = false;
    bool                  dpiUnaware_ = false;

    // 付随 UI (プラグイン GUI の外側に出すもの)
    QPushButton* bypassButton_ = nullptr;
    QLabel*      presetLabel_  = nullptr;
    QPushButton* resetButton_  = nullptr;

#if defined(Q_OS_WIN)
    void* aviutlFilter_ = nullptr;   // FILTER_DLL*
#endif
};

} // namespace yave::plugin
```

---

## 11.8 audio/AudioRenderEngine.h

```cpp
#pragma once

#include "IAudioDevice.h"
#include "AudioClock.h"
#include "AudioRenderGraph.h"
#include "MeterBridge.h"

#include <QObject>

#include <atomic>
#include <memory>
#include <vector>

namespace yave {
class Timeline;
class Project;
}

namespace yave::audio {

/// オーディオ出力の中核。マスタークロックの供給元。
///
/// スレッド安全性:
///   - rtCallback() は OS のリアルタイムスレッドから呼ばれる。
///     malloc / lock / Qt シグナル / 例外 は一切使わない。
///   - それ以外の public メソッドは UI スレッドから呼ぶ。
class AudioRenderEngine : public QObject
{
    Q_OBJECT
public:
    static AudioRenderEngine& instance();

    // ================= デバイス =================

    static std::vector<AudioDeviceInfo> availableDevices();

    bool openDevice(const QString& deviceId = {},   // 空なら既定デバイス
                    int sampleRate = 48000,
                    int bufferFrames = 512,
                    QString* errorOut = nullptr);
    void closeDevice();
    bool isDeviceOpen() const;

    QString deviceId()     const;
    int     sampleRate()   const;
    int     bufferFrames() const;

    /// デバイス出力レイテンシ + プラグイン合計レイテンシ
    int64_t totalLatencySamples() const;

    // ================= 再生制御 =================

    /// 再生を開始する。startFrame はタイムラインのフレーム番号。
    void play(int64_t startFrame);
    void pause();
    void stop();
    /// 再生中のシーク。RT スレッドを止めずに位置を変える。
    void seek(int64_t frame);

    bool isPlaying() const { return playing_.load(std::memory_order_relaxed); }

    /// 現在「耳に届いている」フレーム位置。映像はこれに追従する。
    int64_t currentFrame() const;

    /// ループ再生
    void setLoopRange(const TimeRange& r, bool enabled);

    // ================= グラフ =================

    /// Timeline から AudioRenderGraph を構築して差し替える。
    /// Timeline::structureChanged / トラックのゲイン変更などで呼ばれる。
    /// 再生を止めずに差し替えられる (RCU)。
    void rebuildGraph(const Timeline& timeline, const Project& project);

    /// プラグインのレイテンシ変更通知を受けたときに立てるフラグ。
    /// 次のイベントループで rebuildGraph() が呼ばれる。
    void requestGraphRebuild();

    // ================= PDC =================

    void setPdcEnabled(bool on);
    bool isPdcEnabled() const { return pdcEnabled_; }

    // ================= オフライン =================

    /// 書き出し用。RT 制約なしで同じグラフを回す。
    /// sink は (チャンネル配列, フレーム数) を受け取る。
    void renderOffline(const TimeRange& range, int blockFrames,
                       const std::function<void(const float* const*, int)>& sink,
                       const std::function<bool(double)>& progressFn);

    // ================= メーター =================

    /// 直近のピークレベル (dBFS)。UI スレッドから 30Hz 程度で呼ぶ。
    std::vector<float> masterPeaks() const;
    std::vector<float> trackPeaks(const QUuid& trackId) const;

signals:
    void deviceOpened();
    void deviceClosed();
    void deviceError(const QString& message);
    void playbackStateChanged(bool playing);
    void latencyChanged(int64_t totalSamples);

private:
    AudioRenderEngine();
    ~AudioRenderEngine() override;

    /// リアルタイムコールバック。static にして this を userData で渡す。
    static void rtCallback(float* const* out, int numChannels, int numFrames,
                           void* userData) noexcept;

    void  publishGraph(std::unique_ptr<AudioRenderGraph> g);
    void  collectRetiredGraphs();
    void  ensureScratchCapacity(const AudioRenderGraph& g);

    std::unique_ptr<IAudioDevice>   device_;
    AudioClock                      clock_;
    MeterBridge                     meterBridge_;

    std::atomic<AudioRenderGraph*>  activeGraph_{nullptr};
    std::vector<std::unique_ptr<AudioRenderGraph>> liveGraphs_;
    struct RetiredGraph { std::unique_ptr<AudioRenderGraph> g; uint64_t retiredAtGeneration; };
    std::vector<RetiredGraph>       retired_;
    std::atomic<uint64_t>           rtGeneration_{0};

    std::atomic<bool>               playing_{false};
    std::atomic<bool>               rebuildRequested_{false};
    bool                            pdcEnabled_ = true;
    int64_t                         pluginLatencySamples_ = 0;

    struct Scratch;
    std::unique_ptr<Scratch>        scratch_;    // 事前確保済みバッファ群
};

} // namespace yave::audio
```

---

## 11.9 ai/AiGenerationParams.h

```cpp
#pragma once

#include "../core/TimeRange.h"
#include "../core/Rational.h"

#include <QUuid>
#include <QString>
#include <QSize>
#include <QJsonObject>
#include <QPointF>

#include <optional>
#include <vector>

namespace yave::ai {

/// 何を生成するか
enum class GenerationKind
{
    Video,           ///< 動画
    Audio,           ///< 音声 (ナレーション / 効果音 / BGM)
    Subtitle,        ///< 字幕 (書き起こし / スクリプト生成 / 翻訳)
    Mask,            ///< マスク画像シーケンス
    EffectMetadata,  ///< エフェクトパラメータ (自動カラーグレード等)
    Image            ///< 静止画
};

/// 動画生成のサブモード
enum class VideoGenMode
{
    TextToVideo,     ///< T2V : プロンプトのみから生成
    ImageToVideo,    ///< I2V : 参照画像をベースに生成
    VideoToVideo     ///< V2V : 既存レイヤーの映像をベースにスタイル変換 / アニメ化
};

/// I2V における参照フレームの与え方
enum class I2VReferenceMode
{
    StartFrameOnly,  ///< 開始時点のみ参照 (そこから動き出す)
    EndFrameOnly,    ///< 終了時点のみ参照 (そこへ収束する)
    BothEnds         ///< 両端参照 = 前後キーフレーム間の補間生成
};

enum class AudioGenMode    { Narration, SoundEffect, Bgm, VoiceConversion };
enum class SubtitleGenMode { SpeechToText, ScriptFromPrompt, Translate };

/// 参照画像の指定方法
struct ImageReference
{
    enum class Source
    {
        FilePath,        ///< ファイルから
        TimelineFrame    ///< タイムラインの指定フレームをレンダリングして使う
    };

    Source  source = Source::FilePath;

    QString filePath;                ///< Source::FilePath 用。プロジェクト相対パス
    QUuid   sourceTrackId;           ///< Source::TimelineFrame 用。空なら全レイヤー合成
    int64_t sourceFrame = 0;         ///< Source::TimelineFrame 用
    double  strength    = 1.0;       ///< 参照の効き具合 0..1

    QJsonObject toJson() const;
    static ImageReference fromJson(const QJsonObject& o);
};

/// V2V のソース映像指定
struct VideoReference
{
    QUuid     sourceTrackId;
    TimeRange sourceRange;              ///< 空なら生成区間と同じ
    double    denoiseStrength = 0.6;    ///< 元映像をどれだけ残すか (0=完全保持, 1=完全生成)

    QJsonObject toJson() const;
    static VideoReference fromJson(const QJsonObject& o);
};

/// 生成の全パラメータ。これがそのままプロジェクト JSON に永続化される。
struct AiGenerationParams
{
    // ---------------- 共通 ----------------
    GenerationKind kind = GenerationKind::Video;
    QUuid          targetTrackId;      ///< 生成物を置くトラック
    TimeRange      range;              ///< In / Out (タイムラインのフレーム単位)
    QString        modelId;            ///< "wan2.2-i2v-14b" 等
    QString        providerId;         ///< 空なら ProviderRegistry が自動選択
    QString        prompt;
    QString        negativePrompt;
    int64_t        seed = -1;          ///< -1 = ランダム。生成後に実値を書き戻す
    int            steps = 30;
    double         guidanceScale = 7.5;
    QJsonObject    extraParams;        ///< モデル固有の追加パラメータ

    // ---------------- Video ----------------
    VideoGenMode                  videoMode  = VideoGenMode::TextToVideo;
    I2VReferenceMode              i2vRefMode = I2VReferenceMode::StartFrameOnly;
    std::optional<ImageReference> startReference;
    std::optional<ImageReference> endReference;
    std::optional<VideoReference> videoReference;
    QSize                         outputResolution{1280, 720};
    Rational                      outputFrameRate = timebase::Fps30;
    bool                          loopSeamless = false;

    // ---------------- Audio ----------------
    AudioGenMode audioMode = AudioGenMode::Narration;
    QString      voiceId;
    double       speakingRate = 1.0;
    double       pitch        = 0.0;
    QString      referenceAudioPath;      ///< ボイスクローン用 (プロジェクト相対)
    int          audioSampleRate = 48000;
    int          audioChannels   = 2;
    double       targetLufs      = -16.0; ///< ラウドネス正規化目標

    // ---------------- Subtitle ----------------
    SubtitleGenMode subtitleMode = SubtitleGenMode::SpeechToText;
    QUuid           sourceAudioTrackId;
    QString         language;             ///< "ja" / "en" / "auto"
    QString         targetLanguage;
    bool            wordLevelTimestamps = true;
    QString         subtitleStylePresetId = QStringLiteral("default");
    int             maxCharsPerCue = 0;   ///< 0 = 言語ごとの既定値を使う
    int64_t         minCueDurationFrames = 0;

    // ---------------- Mask ----------------
    QUuid                maskSourceTrackId;
    QString              maskTargetDescription;   ///< "person" / "sky" 等
    std::vector<QPointF> maskHintPoints;          ///< SAM 用のクリックヒント (正規化座標)
    bool                 maskTrackAcrossFrames = true;
    bool                 maskFeather = true;

    // ---------------- 出力の扱い ----------------
    bool replaceExistingClips = false;
    bool createNewTrack       = false;

    // ---------------- シリアライズ ----------------
    QJsonObject toJson() const;
    static AiGenerationParams fromJson(const QJsonObject& o);

    // ---------------- 検証 ----------------
    struct ValidationResult
    {
        bool    ok = true;
        QString errorKey;      ///< "error.ai.missingStartRef" 等の翻訳キー
    };
    ValidationResult validate() const;

    /// キャッシュキーの計算に使う正規化ハッシュ。
    /// 配置先 (targetTrackId 等) は含めない。
    QByteArray contentHash() const;
};

} // namespace yave::ai
```

---

## 11.10 ai/AiGenerationOrchestrator.h

```cpp
#pragma once

#include "AiGenerationParams.h"
#include "AiGenerationTask.h"

#include <QObject>
#include <QThreadPool>
#include <QMutex>
#include <QUuid>

#include <memory>
#include <vector>

namespace yave {
class Project;
namespace subtitle { class SubtitleClip; }
namespace render { class RhiCompositor; }
}

namespace yave::ai {

class ProviderRegistry;
class GenerationCache;

/// 非同期で各生成タスクを管理・実行するコアクラス。
///
/// 責務:
///   - タスクの投入とキューイング
///   - プレースホルダクリップの即時配置
///   - プロバイダの選択と実行
///   - 生成物のキャッシュと Timeline へのコミット (Undo 可能)
///   - セッション復帰
///
/// スレッド:
///   public メソッドは UI スレッドから呼ぶ。
///   実行は QThreadPool 上で行われ、完了通知は queued connection で UI へ戻る。
class AiGenerationOrchestrator : public QObject
{
    Q_OBJECT
public:
    AiGenerationOrchestrator(Project* project,
                             render::RhiCompositor* compositor,
                             QObject* parent = nullptr);
    ~AiGenerationOrchestrator() override;

    // ================= 投入 =================

    /// タスクを投入する。
    /// 妥当性検証に失敗した場合は null QUuid を返し taskFailed を発火する。
    /// 成功時は即座にプレースホルダクリップが配置される (Undo 可能)。
    QUuid submit(const AiGenerationParams& params);

    /// 同一パラメータでの再生成。newSeed=true なら seed を振り直す。
    QUuid regenerate(const QUuid& taskId, bool newSeed);

    // ================= 制御 =================

    void cancel(const QUuid& taskId);
    void cancelAll();
    /// シャットダウン時に呼ぶ。timeoutMs 内に終わらなければ強制打ち切り。
    bool waitForDone(int timeoutMs);

    void retry(const QUuid& taskId);

    // ================= コミット =================

    /// Cached 状態のタスクを Timeline へ反映する。
    /// CommitGeneratedAssetCommand を QUndoStack へ push する。
    void commit(const QUuid& taskId);

    /// 生成物を破棄し、プレースホルダを削除する。
    void discard(const QUuid& taskId);

    /// Cached になった瞬間に自動で commit するか。
    void setAutoCommit(bool on) { autoCommit_ = on; }
    bool autoCommit() const { return autoCommit_; }

    // ================= 照会 =================

    std::vector<AiGenerationTask*> tasks() const;
    AiGenerationTask*              task(const QUuid& id) const;
    int                            activeTaskCount() const;

    ProviderRegistry* providerRegistry() const { return registry_; }
    GenerationCache*  cache() const { return cache_; }

    // ================= 永続化 =================

    /// .yave_cache/tasks.json へ書き出す
    void persistToDisk() const;

    /// 起動時に呼ぶ。Running だったタスクは Queued に戻して再投入する。
    void restoreFromDisk();

    /// プロジェクト JSON への書き出し / 読み込み (9.5)
    QJsonArray serializeTasks() const;
    void       deserializeTasks(const QJsonArray& arr);

signals:
    void taskAdded(const QUuid& id);
    void taskStateChanged(const QUuid& id, TaskState state);
    void taskProgressChanged(const QUuid& id, double progress);
    void taskCached(const QUuid& id);
    void taskCommitted(const QUuid& id);
    void taskFailed(const QUuid& id, const QString& message);
    void activeTaskCountChanged(int count);

private:
    /// ワーカスレッドで実行される本体
    void runTask(AiGenerationTask* task);

    /// 入力の準備 (参照フレーム抽出 / 音声書き出し等)。ワーカスレッド。
    GenerationInput prepareInput(const AiGenerationParams& p);

    /// 生成物の後処理 (fps 合わせ / 尺合わせ / 正規化)。ワーカスレッド。
    std::vector<GeneratedAsset> postProcess(const AiGenerationParams& p,
                                            const GenerationOutput& out);

    /// Cached になったときに UI スレッドで呼ばれる
    void onTaskCached(AiGenerationTask* task);

    /// I2V の参照画像を解決してファイルパスを返す
    QString resolveImageRef(const ImageReference& ref, int64_t defaultFrame,
                            const QDir& workDir, const QString& fileName);

    Project*                                       project_    = nullptr;
    render::RhiCompositor*                         compositor_ = nullptr;
    ProviderRegistry*                              registry_   = nullptr;
    GenerationCache*                               cache_      = nullptr;

    QThreadPool                                    pool_;
    std::vector<std::unique_ptr<AiGenerationTask>> tasks_;
    bool                                           autoCommit_ = false;
    mutable QMutex                                 mutex_;
};

} // namespace yave::ai
```

---

## 11.11 io/ProjectSerializer.h

```cpp
#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>

#include <memory>
#include <vector>

namespace yave {
class Project;
class Timeline;
class Track;
class Clip;
namespace subtitle { class SubtitleClip; struct SubtitleEffectInstance; }
namespace ai       { struct AiGenerationParams; class AiGenerationTask; }
namespace plugin   { class Vst3Host; }
}

namespace yave::io {

struct SaveOptions
{
    bool indented                = true;   ///< false なら Compact
    bool collectGeneratedAssets  = false;  ///< AI 生成物を <project>/assets/generated/ へコピー
    bool collectSourceAssets     = false;  ///< 素材を <project>/assets/ へコピー
    bool embedThumbnails         = false;  ///< サムネイルを Base64 で埋め込む
    bool omitDefaultValues       = true;   ///< 既定値と同じフィールドを省略してサイズを削減
};

struct LoadResult
{
    bool        ok = false;
    QString     errorMessage;
    QStringList warnings;                  ///< 未解決アセット / 未インストールプラグイン等
    int         loadedSchemaVersion = 0;
    bool        migrated = false;
    QStringList missingAssetPaths;
    QStringList missingPluginIds;
};

/// JSON によるプロジェクトの保存・読み込み管理。
///
/// 設計方針:
///   - 無限レイヤーは "tracks" 配列で表現する。配列順 = Z オーダー。
///   - enum は必ず文字列で保存する (数値だと enum への値追加で壊れる)。
///   - パスは常にプロジェクトファイル相対、区切りは '/'。
///   - 認識できないフィールドは保持して書き戻す (前方互換)。
///   - 保存は QSaveFile による原子的書き込み。
class ProjectSerializer
{
public:
    static constexpr int kCurrentSchemaVersion = 1;

    // ================= トップレベル =================

    static bool       save(const Project& project, const QString& path,
                           const SaveOptions& opts, QString* errorOut = nullptr);
    static LoadResult load(Project* project, const QString& path);

    /// 自動保存 (CBOR。速度優先)
    static bool       saveAutosave(const Project& project, const QString& path);
    static LoadResult loadAutosave(Project* project, const QString& path);

    // ================= 個別シリアライズ =================
    // (単体テストしやすいよう public にしている)

    static QJsonObject serializeProject(const Project& p, const SaveOptions& o);
    static QJsonArray  serializeAssets(const Project& p, const SaveOptions& o);
    static QJsonArray  serializeSubtitleStylePresets(const Project& p);

    /// 無限レイヤー: Track の動的リストを QJsonArray にする
    static QJsonArray  serializeTracks(const Timeline& tl, const SaveOptions& o);
    static QJsonObject serializeTrack(const Track& t, const SaveOptions& o);

    static QJsonObject serializeClip(const Clip& c, const SaveOptions& o);
    static QJsonObject serializeSubtitleClip(const subtitle::SubtitleClip& c,
                                             const SaveOptions& o);
    static QJsonArray  serializeSubtitleEffectStack(
                           const std::vector<subtitle::SubtitleEffectInstance>& stack);

    static QJsonObject serializeAiParams(const ai::AiGenerationParams& p);
    static QJsonObject serializeAiTask(const ai::AiGenerationTask& t);
    static QJsonArray  serializeAiTasks(const Project& p);

    static QJsonArray  serializeVst3Chain(const std::vector<plugin::Vst3Host*>& chain);
    static QJsonObject serializeVst3(const plugin::Vst3Host& host);

    // ================= 個別デシリアライズ =================

    static bool deserializeProject(Project* p, const QJsonObject& o, LoadResult* r);
    static void deserializeAssets(Project* p, const QJsonArray& arr, LoadResult* r);
    static void deserializeTracks(Timeline* tl, const QJsonArray& arr,
                                  Project* p, LoadResult* r);
    static std::unique_ptr<Track>  deserializeTrack(const QJsonObject& o,
                                                     Project* p, LoadResult* r);
    static std::shared_ptr<Clip>   deserializeClip(const QJsonObject& o,
                                                    Project* p, LoadResult* r);
    static std::shared_ptr<subtitle::SubtitleClip>
                                   deserializeSubtitleClip(const QJsonObject& o,
                                                            Project* p, LoadResult* r);
    static std::vector<subtitle::SubtitleEffectInstance>
                                   deserializeSubtitleEffectStack(const QJsonArray& arr,
                                                                   LoadResult* r);
    static ai::AiGenerationParams  deserializeAiParams(const QJsonObject& o);

private:
    /// schemaVersion が古い場合にマイグレーションを順次適用する
    static void applyMigrations(QJsonObject& root, int fromVersion, LoadResult* r);

    /// 直近 N 世代のバックアップを回転させる
    static void rotateBackups(const QString& path, int generations);
};

} // namespace yave::io
```

---

## 11.12 補助クラス

### 11.12.1 subtitle/SubtitleEffectInstance.h

```cpp
#pragma once

#include <yave/sdk/ParameterSchema.h>

#include <QUuid>
#include <QString>

namespace yave::sdk { class ISubtitleEffect; }

namespace yave::subtitle {

/// 字幕クリップのエフェクトスタックに積まれる 1 要素。
struct SubtitleEffectInstance
{
    // ---- 永続化される ----
    QUuid                      instanceId;
    QString                    effectId;      ///< "yave.typewriter" / "com.example.glitch"
    QString                    pluginId;      ///< 組み込みなら空文字列
    bool                       enabled = true;
    yave::sdk::ParameterValues params;

    // ---- 実行時のみ (永続化しない) ----
    /// このインスタンス専用のエフェクト実装。
    /// prepare() の前計算結果を内部に持つため、クリップ間で共有しない。
    /// PluginManager::createSubtitleEffect() で生成する。
    plugin::SubtitleEffectPtr   effect;
    bool                        prepared = false;    ///< prepare() 済みか
    bool                        missing  = false;    ///< プラグイン未インストール

    /// レイアウトが変わったら prepare() をやり直す必要がある。
    /// SubtitleClip::contentRevision() と比較して判定する。
    uint64_t                    preparedForRevision = 0;

    SubtitleEffectInstance() = default;
    SubtitleEffectInstance(SubtitleEffectInstance&&) = default;
    SubtitleEffectInstance& operator=(SubtitleEffectInstance&&) = default;
    SubtitleEffectInstance(const SubtitleEffectInstance&) = delete;

    /// 永続データのみを複製し、effect は新規生成する (クリップ複製時に使う)。
    SubtitleEffectInstance cloneForNewClip() const;

    static SubtitleEffectInstance create(const QString& effectId,
                                         const QString& pluginId = {});
};

} // namespace yave::subtitle
```

### 11.12.2 i18n/LanguageManager.h

[10.4](10-i18n.md) に全文を掲載。

### 11.12.3 core/commands/UndoCommandBase.h

```cpp
#pragma once

#include <QUndoCommand>
#include <QString>

namespace yave {

class Project;

/// すべての編集コマンドの基底。
/// 目的:
///   - コマンド ID による自動マージ (連続したドラッグを 1 コマンドにまとめる)
///   - 実行後の副作用通知 (音声グラフ再構築の要求など) を一元化
class UndoCommandBase : public QUndoCommand
{
public:
    enum CommandId
    {
        IdNone           = -1,
        IdMoveClip       = 1,
        IdTrimClip       = 2,
        IdChangeOpacity  = 3,
        IdChangeGain     = 4,
        IdEditSubtitleText = 5,
        IdChangeEffectParam = 6,
    };

    explicit UndoCommandBase(Project* project, const QString& text);

    int  id() const override { return IdNone; }
    bool mergeWith(const QUndoCommand* other) override { Q_UNUSED(other); return false; }

    void redo() override;
    void undo() override;

protected:
    /// 派生クラスが実装する実処理
    virtual void doRedo() = 0;
    virtual void doUndo() = 0;

    /// この操作が音声グラフの再構築を要求するか
    virtual bool affectsAudioGraph() const { return false; }
    /// この操作がレンダリングスナップショットを無効化するか
    virtual bool affectsRendering() const { return true; }

    Project* project() const { return project_; }

private:
    void notifyAfterChange();

    Project* project_ = nullptr;
};

} // namespace yave
```

---

## 11.13 クラス間の所有関係まとめ

```
Project (所有)
 ├── Timeline (unique_ptr)
 │     └── std::vector<std::unique_ptr<Track>>       ← 無限レイヤー
 │           └── std::vector<std::shared_ptr<Clip>>
 │                 └── SubtitleClip
 │                       └── std::vector<SubtitleEffectInstance>
 │                             └── ISubtitleEffect*  (Registry 所有。ここは弱参照)
 ├── AssetLibrary (unique_ptr)
 ├── QUndoStack (unique_ptr)
 │     └── QUndoCommand (Undo 用に Clip の shared_ptr を保持しうる)
 ├── AiGenerationOrchestrator (unique_ptr)
 │     └── std::vector<std::unique_ptr<AiGenerationTask>>
 └── SubtitleStylePresetTable

PluginManager (singleton, 所有)
 ├── Vst3Registry
 ├── SubtitleEffectRegistry
 │     └── std::vector<std::unique_ptr<LoadedSubtitlePlugin>>
 │           └── ISubtitleEffect (実体)
 └── AviUtlRegistry (Windows のみ)

AudioRenderEngine (singleton, 所有)
 └── AudioRenderGraph (RCU で差し替え)
       └── TrackNode
             └── Vst3ProcessorNode*  (Track 所有。ここは弱参照)
```

> **`Clip` を `shared_ptr` にしている理由**: Undo コマンドが「削除したクリップ」を
> 保持し続ける必要がある。`unique_ptr` だと Timeline から外した瞬間に
> 所有権をコマンドへ移す必要があり、Redo/Undo の往復で所有権の移動が複雑になる。
>
> **`ISubtitleEffect` はクリップごとに別インスタンスを作る**:
> `prepare()` はグリフ数に依存した前計算結果をインスタンス内部に保持する
> ([12.5.1](12-snippets.md) の `order_` がそれにあたる)。
> したがって 1 個の実装インスタンスを複数のクリップで共有することはできない。
>
> `SubtitleEffectRegistry` は**プロトタイプ**を 1 個だけ保持し
> (一覧表示と `parameterSchema()` の取得に使う)、
> 実際にクリップへ積むときは `createSubtitleEffect()` で
> **新しいインスタンスを生成**する。
> `SubtitleEffectInstance` がそのインスタンスを所有する。
>
> ```cpp
> // SubtitleEffectInstance が持つ所有ポインタ
> using EffectPtr = std::unique_ptr<yave::sdk::ISubtitleEffect,
>                                   std::function<void(yave::sdk::ISubtitleEffect*)>>;
> ```
>
> 外部プラグインは `SubtitleEffectFactoryV1::destroyEffect` で解放する必要があるため、
> 素の `unique_ptr` ではなく deleter を持たせる。
