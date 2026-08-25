#include "LayerPass.h"
#include "ColorSpace.h"

#include "../util/Log.h"

#include <QList>

#include <rhi/qrhi.h>

namespace yave::render {

// layer_blend.frag の uniform ブロックと一致させること (3.4.3 参照)
struct alignas(16) LayerUniforms
{
    QMatrix4x4 transform;
    QVector4D  cropRect;      // x, y, w, h  (0..1)
    float      opacity;
    int        blendMode;
    int        colorSpace;    // 0=RGB(full), 1=BT709 limited, 2=BT709 full, 3=BT2020
    float      pad;
};

struct LayerPass::Impl
{
    QRhi* rhi = nullptr;

    std::unique_ptr<QRhiGraphicsPipeline> pipeline;
    std::unique_ptr<QRhiShaderResourceBindings> srb;
    std::unique_ptr<QRhiBuffer> ubuf;
    std::unique_ptr<QRhiBuffer> vbuf;
    std::unique_ptr<QRhiSampler> sampler;

    bool initialized = false;
    bool pipelineReady = false;
    QRhiRenderPassDescriptor* lastRpDesc = nullptr;

    void bindTextures(QRhiTexture* src, QRhiTexture* dst)
    {
        QList<QRhiShaderResourceBinding> bindings;
        bindings.append(QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            ubuf.get()));
        bindings.append(QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage, src, sampler.get()));
        bindings.append(QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage,
            dst ? dst : src, sampler.get()));
        srb->setBindings(bindings.cbegin(), bindings.cend());
        srb->create();
    }
};

LayerPass::LayerPass() : impl_(std::make_unique<Impl>()) {}
LayerPass::~LayerPass()
{
    releaseResources();
}

void LayerPass::releaseResources()
{
    if (!impl_)
        return;
    impl_->pipeline.reset();
    impl_->srb.reset();
    impl_->ubuf.reset();
    impl_->vbuf.reset();
    impl_->sampler.reset();
    impl_->initialized = false;
}

bool LayerPass::initialize(void* rhiPtr, void*,
                           const QByteArray& qsbVert, const QByteArray& qsbFrag)
{
    auto* rhi = static_cast<QRhi*>(rhiPtr);
    impl_->rhi = rhi;

    const QShader vs = QShader::fromSerialized(qsbVert);
    const QShader fs = QShader::fromSerialized(qsbFrag);
    if (!vs.isValid() || !fs.isValid()) {
        qCWarning(lcRender) << "Invalid shader blobs for layer pass";
        return false;
    }

    // 頂点バッファ: フルスクリーン三角形 (UV 付き、3 頂点で矩形を覆う)
    static const float vertexData[] = {
        // x, y, u, v
        -1.0f, -1.0f, 0.0f, 1.0f,
         3.0f, -1.0f, 2.0f, 1.0f,
        -1.0f,  3.0f, 0.0f, -1.0f,
    };

    impl_->vbuf.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                     sizeof(vertexData)));
    if (!impl_->vbuf || !impl_->vbuf->create())
        return false;

    impl_->ubuf.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                     sizeof(LayerUniforms)));
    if (!impl_->ubuf || !impl_->ubuf->create())
        return false;

    impl_->sampler.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                         QRhiSampler::None, QRhiSampler::ClampToEdge,
                                         QRhiSampler::ClampToEdge));
    if (!impl_->sampler || !impl_->sampler->create())
        return false;

    impl_->srb.reset(rhi->newShaderResourceBindings());
    impl_->bindTextures(nullptr, nullptr);

    impl_->pipeline.reset(rhi->newGraphicsPipeline());
    impl_->pipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vs },
        { QRhiShaderStage::Fragment, fs },
    });
    impl_->pipeline->setCullMode(QRhiGraphicsPipeline::None);
    impl_->pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    impl_->pipeline->setSampleCount(1);

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({ { 4 * sizeof(float) } });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) },
    });
    impl_->pipeline->setVertexInputLayout(inputLayout);
    impl_->pipeline->setShaderResourceBindings(impl_->srb.get());

    // パイプラインの create() は最初の renderTarget が判明した後で行う
    impl_->initialized = true;
    return true;
}

void LayerPass::draw(void* commandBufferPtr, void* renderTargetPtr,
                     void* srcTexturePtr, void* dstTexturePtr, const LayerItem& layer)
{
    auto* cb = static_cast<QRhiCommandBuffer*>(commandBufferPtr);
    auto* rt = static_cast<QRhiRenderTarget*>(renderTargetPtr);
    auto* src = static_cast<QRhiTexture*>(srcTexturePtr);
    auto* dst = static_cast<QRhiTexture*>(dstTexturePtr);

    if (!impl_->initialized || !cb || !rt || !src) {
        qCWarning(lcRender, "LayerPass not fully initialized; skipping layer");
        return;
    }

    if (!impl_->pipeline) {
        qCWarning(lcRender, "Pipeline missing");
        return;
    }

    // ping-pong の 2 つの RT はフォーマット / サンプル数が同一のため、
    // パイプラインは互換な rp 記述子で 1 回だけ生成すればよい。
    if (!impl_->pipelineReady) {
        impl_->pipeline->setRenderPassDescriptor(rt->renderPassDescriptor());
        impl_->pipelineReady = impl_->pipeline->create();
        impl_->lastRpDesc = rt->renderPassDescriptor();
    }

    // uniform 更新
    LayerUniforms u;
    u.transform   = layer.transform;
    u.cropRect    = QVector4D(float(layer.cropRect.x()), float(layer.cropRect.y()),
                              float(layer.cropRect.width()),
                              float(layer.cropRect.height()));
    u.opacity     = layer.opacity;
    u.blendMode   = int(layer.blendMode);
    u.colorSpace  = 0;   ///< RGB (YUV ソースは事前変換 or 専用パス)

    QRhiResourceUpdateBatch* batch = impl_->rhi->nextResourceUpdateBatch();
    batch->updateDynamicBuffer(impl_->ubuf.get(), 0, sizeof(u), &u);

    // テクスチャを SRB へ結び直す (毎フレーム差し替え)
    impl_->bindTextures(src, dst ? dst : src);

    cb->beginPass(rt, QColor::fromRgbF(0.0f, 0.0f, 0.0f, 0.0f),
                  QRhiDepthStencilClearValue(1.0f, 0), batch);
    cb->setGraphicsPipeline(impl_->pipeline.get());
    cb->setViewport(QRhiViewport(0, 0, float(rt->pixelSize().width()),
                                 float(rt->pixelSize().height())));
    cb->setShaderResources(impl_->srb.get());
    const QRhiCommandBuffer::VertexInput vb(impl_->vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vb);
    cb->draw(3);
    cb->endPass();
}

} // namespace yave::render
