#pragma once

#include "RenderSnapshot.h"
#include "TimeRange.h"
#include "VideoFilter.h"

#include <QJsonObject>
#include <QMatrix4x4>
#include <QRectF>
#include <QUuid>

#include <memory>

namespace yave {

class Track;

enum class ClipType { Video, Audio, Subtitle, AiPlaceholder, Image, Color, Title };

/// タイムライン上に置かれる編集単位の基底クラス。
/// 派生: VideoClip / AudioClip / SubtitleClip / AiPlaceholderClip / ColorClip
///
/// Clip を shared_ptr にしている理由:
///   Undo コマンドが「削除したクリップ」を保持し続ける必要があるため。
///   unique_ptr だと Timeline から外した瞬間に所有権をコマンドへ移すことになり、
///   Undo/Redo の往復で所有権の移動が複雑になる。
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
    QString name() const { return name_; }
    void    setName(const QString& n) { name_ = n; }

    bool isEnabled() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }

    bool isLocked() const { return locked_; }
    void setLocked(bool l) { locked_ = l; }

    // --- 合成 ---
    double opacity() const { return opacity_; }
    void   setOpacity(double o) { opacity_ = qBound(0.0, o, 1.0); }

    BlendMode blendMode() const { return blendMode_; }
    void      setBlendMode(BlendMode m) { blendMode_ = m; }

    const QMatrix4x4& transform() const { return transform_; }
    void              setTransform(const QMatrix4x4& m) { transform_ = m; }

    const QRectF& cropRect() const { return crop_; }
    void          setCropRect(const QRectF& r) { crop_ = r; }

    // --- フェード ---
    int64_t fadeInFrames()  const { return fadeIn_; }
    int64_t fadeOutFrames() const { return fadeOut_; }
    void    setFadeInFrames(int64_t f)  { fadeIn_  = f > 0 ? f : 0; }
    void    setFadeOutFrames(int64_t f) { fadeOut_ = f > 0 ? f : 0; }

    /// フェードを加味した最終不透明度
    double effectiveOpacity(int64_t frame) const;

    // --- ビデオフィルタースタック (3.9) ---
    /// opacity / transform と同じ「合成のしかたを決める値」の一員として基底に置く。
    /// 適用順は配列順。無効な段は飛ばす。
    const std::vector<VideoFilterInstance>& filters() const { return filters_; }
    void addFilter(VideoFilterInstance inst) { filters_.push_back(std::move(inst)); }
    void insertFilter(int index, VideoFilterInstance inst);
    void removeFilter(int index);
    void moveFilter(int from, int to);
    void setFilterEnabled(int index, bool enabled);
    void setFilters(std::vector<VideoFilterInstance> f) { filters_ = std::move(f); }

    /// Render Thread へ渡す形へ解決する (無効な段は落とす)。
    std::vector<ResolvedFilter> resolvedFilters() const;

    // --- AI 由来の記録 ---
    QUuid generatedByTaskId() const { return generatedByTaskId_; }
    void  setGeneratedByTaskId(const QUuid& id) { generatedByTaskId_ = id; }
    bool  isAiGenerated() const { return !generatedByTaskId_.isNull(); }

    // --- レンダリング ---
    /// RenderSnapshot に載せる 1 レイヤー分の情報を作る。
    /// frame はタイムライン絶対フレーム。
    virtual LayerItem makeLayerItem(int64_t frame, int zIndex, const Track& track) const = 0;

    // --- 永続化 (未知フィールドの保持。9.11.2 参照) ---
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
    std::vector<VideoFilterInstance> filters_;
    QJsonObject unknownFields_;
};

} // namespace yave
