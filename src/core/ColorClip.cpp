#include "ColorClip.h"
#include "Track.h"

namespace yave {

/// 単色クリップはソースを持たないため、GeneratedSourceRef で
/// 色を伝達する (Render 側がシェーダ / クリアカラーとして扱う)。
LayerItem ColorClip::makeLayerItem(int64_t frame, int zIndex, const Track& track) const
{
    LayerItem item;
    item.clipId     = id();
    item.trackType  = track.type();
    item.zIndex     = zIndex;
    item.blendMode  = blendMode();
    item.opacity    = float(effectiveOpacity(frame));
    item.transform  = transform();

    GeneratedSourceRef ref;
    ref.filePath          = QStringLiteral("color:%1").arg(color_.name(QColor::HexArgb));
    ref.sourceFrameIndex  = 0;
    item.source = std::move(ref);
    Q_UNUSED(frame);
    return item;
}

} // namespace yave
