#include "PreviewItem.h"

#include "../../core/Project.h"
#include "../../core/Timeline.h"
#include "../../render/RhiCompositor.h"
#include "../../util/Log.h"

namespace yave {

// ===========================================================================
//  Renderer
// ===========================================================================

class PreviewRenderer : public QQuickRhiItemRenderer
{
public:
    void initialize(QRhiCommandBuffer* cb) override
    {
        Q_UNUSED(cb);
        if (rhi() && !compositor_) {
            compositor_ = std::make_unique<render::RhiCompositor>();
            // Qt Quick のスワップチェーン形式をそのまま使う
            compositor_->initialize(rhi(), colorTexture()
                                              ? static_cast<void*>(colorTexture())
                                              : nullptr);
        }
    }

    void synchronize(QQuickRhiItem* item) override
    {
        // UI スレッド。RenderSnapshot をコピーする唯一のタイミング。
        auto* preview = static_cast<PreviewItem*>(item);
        if (!preview || !preview->timeline())
            return;

        pendingSnapshot_ = preview->timeline()->buildSnapshot(preview->frameIndex());
    }

    void render(QRhiCommandBuffer* cb) override
    {
        Q_UNUSED(cb);
        if (!compositor_)
            return;
        void* result = compositor_->renderFrame(pendingSnapshot_);
        Q_UNUSED(result);
        // 実際の表示は composite 結果を colorTexture へスケール描画する。
        // 現行ビルドでは合成パイプラインの骨格のみが稼働する。
    }

private:
    std::unique_ptr<render::RhiCompositor> compositor_;
    RenderSnapshot pendingSnapshot_;
};

// ===========================================================================
//  PreviewItem
// ===========================================================================

PreviewItem::PreviewItem() = default;

void PreviewItem::setFrameIndex(qint64 f)
{
    if (frameIndex_ == f)
        return;
    frameIndex_ = f;
    update();
    emit frameIndexChanged();
}

double PreviewItem::zoom() const
{
    return zoom_;
}

void PreviewItem::setZoom(double z)
{
    z = qBound(0.05, z, 8.0);
    if (qFuzzyCompare(zoom_, z))
        return;
    zoom_ = z;
    emit zoomChanged();
}

void PreviewItem::attachTimeline(Timeline* timeline)
{
    timeline_ = timeline;
}

QQuickRhiItemRenderer* PreviewItem::createRenderer()
{
    return new PreviewRenderer();
}

} // namespace yave
