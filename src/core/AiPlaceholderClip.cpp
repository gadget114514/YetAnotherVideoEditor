#include "AiPlaceholderClip.h"
#include "Track.h"

namespace yave {

LayerItem AiPlaceholderClip::makeLayerItem(int64_t frame, int zIndex, const Track& track) const
{
    LayerItem item;
    item.clipId    = id();
    item.trackType = track.type();
    item.zIndex    = zIndex;
    item.blendMode = blendMode();
    // プレースホルダ自体は描画しない。UI (QML) が進捗を表示する。
    // Render 側での判別用に opacity 0 のアイテムだけ載せる。
    item.opacity   = 0.0f;

    GeneratedSourceRef ref;
    ref.filePath          = QStringLiteral("placeholder:%1").arg(taskId_.toString(QUuid::WithoutBraces));
    item.source = std::move(ref);
    Q_UNUSED(frame);
    return item;
}

} // namespace yave
