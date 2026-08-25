#include "AudioClip.h"
#include "Track.h"

namespace yave {

AudioClip::AudioClip(const QUuid& assetId) : assetId_(assetId) {}

/// 音声クリップは映像合成レイヤーには参加しない。
/// monostate の LayerItem を返す (オーディオグラフ側で参照される)。
LayerItem AudioClip::makeLayerItem(int64_t, int zIndex, const Track& track) const
{
    LayerItem item;
    item.clipId    = id();
    item.trackType = track.type();
    item.zIndex    = zIndex;
    return item;
}

} // namespace yave
