#pragma once

#include "Clip.h"
#include "TimeRange.h"
#include "TrackType.h"
#include "Transition.h"
#include "BlendMode.h"

#include <QColor>
#include <QString>
#include <QUuid>

#include <functional>
#include <memory>
#include <vector>

namespace yave {

class IAudioEffectNode;

/// タイムライン上の 1 レイヤー。
///
/// 不変条件:
///   (1) clips_ は range().start の昇順にソートされている
///   (2) clips_ の各要素の TimeRange は互いに重ならない
///   (3) すべての clip の duration > 0
///   (4) transitions_ の各要素は隣接クリップの境界にのみ置かれる (3.10)。
///       1 つの境界に 2 つ以上は置けない
///   (5) トランジションの長さは両側クリップのハンドルの 2 倍を超えない
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
    void   setGain(double g) { gain_ = g < 0.0 ? 0.0 : g; }
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
    std::shared_ptr<Clip> clipAt(int64_t frame) const;                     // O(log n)
    std::shared_ptr<Clip> clipById(const QUuid& id) const;                 // O(n)
    std::vector<std::shared_ptr<Clip>> clipsIn(const TimeRange& r) const;  // O(log n + k)

    /// frame より後ろにある最初のクリップの開始フレーム / 直前のクリップ終端。無ければ -1。
    int64_t nextClipStart(int64_t frame) const;
    int64_t prevClipEnd(int64_t frame) const;

    /// 指定フレームで空いている次の挿入位置を返す (duration 分の空きが必要)。
    int64_t findFreeStart(int64_t fromFrame, int64_t duration) const;

    const std::vector<std::shared_ptr<Clip>>& clips() const { return clips_; }
    size_t  clipCount() const { return clips_.size(); }
    int64_t contentDuration() const;    ///< 最後のクリップの end()

    /// クリップの範囲が外部で変更された後に呼び、ソート順を復元する。
    void resort();

    // ================= トランジション (3.10) =================

    const std::vector<Transition>& transitions() const { return transitions_; }
    void setTransitions(std::vector<Transition> t);   ///< 読み込み時のみ。検証してから入れる

    /// frame がトランジション区間に入っていれば返す。無ければ nullptr。
    const Transition* transitionAt(int64_t frame) const;

    /// boundaryFrame ちょうどの境界に置かれているものを返す。無ければ nullptr。
    const Transition* transitionAtBoundary(int64_t boundaryFrame) const;

    /// 不変条件 (4)(5) を検証して追加する。長さがハンドルを超える場合は
    /// 収まる長さへ自動的に縮める。同じ境界に既にあれば置き換える。
    /// 置けない場合は false を返し、errorOut に理由 (翻訳済み) を入れる。
    bool addTransition(Transition t, QString* errorOut = nullptr);

    void removeTransition(const QUuid& id);

    /// 参照先クリップが消えた / 境界がずれたトランジションを取り除く。
    /// クリップの追加・削除・移動のあとに必ず呼ぶ。
    void dropOrphanTransitions();

    /// その境界に置けるトランジションの最大長 (フレーム)。置けないなら 0。
    int64_t maxTransitionDuration(int64_t boundaryFrame) const;

    /// boundaryFrame で終わる / 始まるクリップ。無ければ nullptr。
    std::shared_ptr<Clip> clipEndingAt(int64_t boundaryFrame) const;
    std::shared_ptr<Clip> clipStartingAt(int64_t boundaryFrame) const;

    // --- エフェクトチェーン (音声トラック) ---
    const std::vector<IAudioEffectNode*>& effectChain() const { return effectChain_; }
    void    addEffect(IAudioEffectNode* fx);
    void    removeEffect(int index);
    void    moveEffect(int from, int to);
    int64_t totalLatencySamples() const;

    // --- デバッグ ---
    void assertInvariants() const;      ///< Debug ビルドのみ実体を持つ

private:
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

    std::vector<std::shared_ptr<Clip>> clips_;        ///< start 昇順
    std::vector<Transition>            transitions_;  ///< クリップ境界に付く (3.10)
    std::vector<IAudioEffectNode*>     effectChain_;  ///< 所有はしない
};

} // namespace yave
