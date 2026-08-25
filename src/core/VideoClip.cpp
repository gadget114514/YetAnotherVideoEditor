#include "VideoClip.h"
#include "Track.h"

namespace yave {

VideoClip::VideoClip(const QUuid& assetId) : assetId_(assetId) {}

int64_t VideoClip::mapToSourceFrame(int64_t timelineFrame) const
{
    const int64_t rel = timelineFrame - range().start;
    const int64_t src = sourceOffset_
                        + int64_t(double(rel) * speed_);
    if (!reversed_)
        return src > 0 ? src : 0;
    // 逆再生: ソースの末尾から遡る。総尺は maxDurationFrames_ を使う。
    const int64_t total = maxDurationFrames_ > 0 ? maxDurationFrames_ : src;
    return qBound<int64_t>(0, total - src, total > 0 ? total - 1 : 0);
}

LayerItem VideoClip::makeLayerItem(int64_t frame, int zIndex, const Track& track) const
{
    LayerItem item;
    item.clipId     = id();
    item.trackType  = track.type();
    item.zIndex     = zIndex;
    item.blendMode  = blendMode();
    item.opacity    = float(effectiveOpacity(frame));
    item.transform  = transform();
    item.cropRect   = cropRect();

    VideoSourceRef ref;
    ref.assetId           = assetId_;
    ref.sourceFrameIndex  = mapToSourceFrame(frame);
    ref.speed             = speed_;
    item.source = std::move(ref);
    return item;
}

} // namespace yave
