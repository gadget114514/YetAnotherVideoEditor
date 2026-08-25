#pragma once

#include "Clip.h"

#include <QUuid>

namespace yave {

/// ビデオクリップ。素材 (assetId) の区間をタイムラインに配置する。
class VideoClip : public Clip
{
public:
    VideoClip() = default;
    explicit VideoClip(const QUuid& assetId);

    ClipType type() const override { return ClipType::Video; }

    std::shared_ptr<Clip> clone() const override
    {
        auto c = std::shared_ptr<VideoClip>(new VideoClip(*this));
        c->setId(QUuid::createUuid());
        return c;
    }

    // --- ソース ---
    QUuid   assetId() const { return assetId_; }
    void    setAssetId(const QUuid& id) { assetId_ = id; }

    int64_t sourceOffset() const override { return sourceOffset_; }
    void    setSourceOffset(int64_t off) override { sourceOffset_ = off > 0 ? off : 0; }
    void    shiftSourceOffset(int64_t delta) override { setSourceOffset(sourceOffset_ + delta); }

    /// ソースの総フレーム数。-1 なら無制限 (画像など)。
    int64_t maxDurationFrames() const { return maxDurationFrames_; }
    void    setMaxDurationFrames(int64_t f) { maxDurationFrames_ = f; }
    int64_t maxDuration() const override { return maxDurationFrames_; }

    // --- 再生速度 ---
    double speed() const { return speed_; }
    void   setSpeed(double s) { speed_ = qBound(0.01, s, 100.0); }

    bool isReversed() const { return reversed_; }
    void setReversed(bool r) { reversed_ = r; }

    /// タイムライン上のフレームをソース内フレームへ変換する (speed / reverse 適用済み)
    int64_t mapToSourceFrame(int64_t timelineFrame) const;

    LayerItem makeLayerItem(int64_t frame, int zIndex, const Track& track) const override;

private:
    QUuid   assetId_;
    int64_t sourceOffset_      = 0;
    int64_t maxDurationFrames_ = -1;
    double  speed_             = 1.0;
    bool    reversed_          = false;
};

} // namespace yave
