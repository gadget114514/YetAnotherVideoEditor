#include "FilterPass.h"

#include "../core/VideoFilter.h"
#include "../util/Log.h"

#include <QList>
#include <QVector4D>

#include <rhi/qrhi.h>

namespace yave::render {

namespace {

/// filter_color.frag / filter_blur.frag の uniform ブロックと一致させること。
struct alignas(16) FilterUniforms
{
    QVector4D p0;
    QVector4D p1;
    QVector4D texel;      // xy = 1/size
    qint32    kind    = 0;
    qint32    subPass = 0;
    qint32    pad0    = 0;
    qint32    pad1    = 0;
};

/// filter_color.frag の kind。-1 は色系シェーダで扱わない (blur)。
int colorFilterKind(const QString& filterId)
{
    if (filterId == QLatin1String(builtinFilter::kColorAdjust)) return 0;
    if (filterId == QLatin1String(builtinFilter::kMono))        return 1;
    if (filterId == QLatin1String(builtinFilter::kSepia))       return 2;
    return -1;
}

bool isBlur(const ResolvedFilter& f)
{
    return f.filterId == QLatin1String(builtinFilter::kBlur);
}

} // namespace

struct FilterPass::Impl
{
    QRhi* rhi = nullptr;

    std::unique_ptr<QRhiBuffer>  vbuf;
    std::unique_ptr<QRhiBuffer>  ubuf;
    std::unique_ptr<QRhiSampler> sampler;

    // 色系 / ぼかしでシェーダが違うため、パイプラインも 2 本持つ。
    std::unique_ptr<QRhiShaderResourceBindings> srb;
    std::unique_ptr<QRhiGraphicsPipeline> colorPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> blurPipeline;

    QShader vs, fsColor, fsBlur;
    bool initialized  = false;
    bool pipelineReady = false;

    void bindTexture(QRhiTexture* src)
    {
        QList<QRhiShaderResourceBinding> bindings;
        bindings.append(QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            ubuf.get()));
        bindings.append(QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage, src, sampler.get()));
        srb->setBindings(bindings.cbegin(), bindings.cend());
        srb->create();
    }

    std::unique_ptr<QRhiGraphicsPipeline> makePipeline(const QShader& fs,
                                                       QRhiRenderPassDescriptor* rp)
    {
        std::unique_ptr<QRhiGraphicsPipeline> pl(rhi->newGraphicsPipeline());
        pl->setShaderStages({ { QRhiShaderStage::Vertex, vs },
                              { QRhiShaderStage::Fragment, fs } });
        pl->setCullMode(QRhiGraphicsPipeline::None);
        pl->setTopology(QRhiGraphicsPipeline::Triangles);
        pl->setSampleCount(1);

        QRhiVertexInputLayout layout;
        layout.setBindings({ { 2 * sizeof(float) } });
        layout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float2, 0 } });
        pl->setVertexInputLayout(layout);
        pl->setShaderResourceBindings(srb.get());
        pl->setRenderPassDescriptor(rp);
        if (!pl->create())
            return nullptr;
        return pl;
    }
};

FilterPass::FilterPass() : impl_(std::make_unique<Impl>()) {}

FilterPass::~FilterPass()
{
    releaseResources();
}

void FilterPass::releaseResources()
{
    if (!impl_)
        return;
    impl_->colorPipeline.reset();
    impl_->blurPipeline.reset();
    impl_->srb.reset();
    impl_->ubuf.reset();
    impl_->vbuf.reset();
    impl_->sampler.reset();
    impl_->initialized   = false;
    impl_->pipelineReady = false;
}

bool FilterPass::initialize(void* rhiPtr, const QByteArray& qsbVert,
                            const QByteArray& qsbFragColor, const QByteArray& qsbFragBlur)
{
    auto* rhi = static_cast<QRhi*>(rhiPtr);
    impl_->rhi = rhi;

    impl_->vs      = QShader::fromSerialized(qsbVert);
    impl_->fsColor = QShader::fromSerialized(qsbFragColor);
    impl_->fsBlur  = QShader::fromSerialized(qsbFragBlur);
    if (!impl_->vs.isValid() || !impl_->fsColor.isValid() || !impl_->fsBlur.isValid()) {
        qCWarning(lcRender) << "Invalid shader blobs for filter pass";
        return false;
    }

    // フルスクリーン三角形 (クリップ空間そのまま)
    static const float vertexData[] = {
        -1.0f, -1.0f,
         3.0f, -1.0f,
        -1.0f,  3.0f,
    };
    impl_->vbuf.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                     sizeof(vertexData)));
    if (!impl_->vbuf || !impl_->vbuf->create())
        return false;

    impl_->ubuf.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                     sizeof(FilterUniforms)));
    if (!impl_->ubuf || !impl_->ubuf->create())
        return false;

    impl_->sampler.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                         QRhiSampler::None, QRhiSampler::ClampToEdge,
                                         QRhiSampler::ClampToEdge));
    if (!impl_->sampler || !impl_->sampler->create())
        return false;

    impl_->srb.reset(rhi->newShaderResourceBindings());
    impl_->bindTexture(nullptr);

    impl_->initialized = true;
    return true;
}

int FilterPass::subPassCount(const ResolvedFilter& filter)
{
    return isBlur(filter) ? 2 : 1;   // 分離ガウシアンは水平 / 垂直の 2 パス
}

void FilterPass::draw(void* commandBufferPtr, void* renderTargetPtr, void* srcTexturePtr,
                      const ResolvedFilter& filter, int subPass)
{
    auto* cb  = static_cast<QRhiCommandBuffer*>(commandBufferPtr);
    auto* rt  = static_cast<QRhiRenderTarget*>(renderTargetPtr);
    auto* src = static_cast<QRhiTexture*>(srcTexturePtr);

    if (!impl_->initialized || !cb || !rt || !src)
        return;

    const bool blur = isBlur(filter);
    const int  kind = colorFilterKind(filter.filterId);
    if (!blur && kind < 0)
        return;   // 未知 / プラグイン由来のフィルタはここでは描かない (8章)

    if (!impl_->pipelineReady) {
        auto* rp = rt->renderPassDescriptor();
        impl_->colorPipeline = impl_->makePipeline(impl_->fsColor, rp);
        impl_->blurPipeline  = impl_->makePipeline(impl_->fsBlur, rp);
        impl_->pipelineReady = impl_->colorPipeline && impl_->blurPipeline;
        if (!impl_->pipelineReady) {
            qCWarning(lcRender) << "FilterPass: failed to create pipelines";
            return;
        }
    }

    const QSize size = rt->pixelSize();

    FilterUniforms u;
    u.p0      = QVector4D(filter.params[0], filter.params[1], filter.params[2], filter.params[3]);
    u.p1      = QVector4D(filter.params[4], filter.params[5], filter.params[6], filter.params[7]);
    u.texel   = QVector4D(size.width()  > 0 ? 1.0f / float(size.width())  : 0.0f,
                          size.height() > 0 ? 1.0f / float(size.height()) : 0.0f,
                          0.0f, 0.0f);
    u.kind    = blur ? 0 : kind;
    u.subPass = subPass;

    QRhiResourceUpdateBatch* batch = impl_->rhi->nextResourceUpdateBatch();
    batch->updateDynamicBuffer(impl_->ubuf.get(), 0, sizeof(u), &u);

    impl_->bindTexture(src);

    cb->beginPass(rt, QColor::fromRgbF(0.0f, 0.0f, 0.0f, 0.0f),
                  QRhiDepthStencilClearValue(1.0f, 0), batch);
    cb->setGraphicsPipeline(blur ? impl_->blurPipeline.get() : impl_->colorPipeline.get());
    cb->setViewport(QRhiViewport(0, 0, float(size.width()), float(size.height())));
    cb->setShaderResources(impl_->srb.get());
    const QRhiCommandBuffer::VertexInput vb(impl_->vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vb);
    cb->draw(3);
    cb->endPass();
}

} // namespace yave::render
