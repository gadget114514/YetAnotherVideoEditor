#include "TransitionPass.h"

#include "../util/Log.h"

#include <QList>
#include <QVector4D>

#include <rhi/qrhi.h>

namespace yave::render {

namespace {

/// transition.frag の uniform ブロックと一致させること。
struct alignas(16) TransitionUniforms
{
    QVector4D params;
    QVector4D fillColor;
    float     progress = 0.0f;
    qint32    mode     = 0;
    qint32    pad0     = 0;
    qint32    pad1     = 0;
};

} // namespace

struct TransitionPass::Impl
{
    QRhi* rhi = nullptr;

    std::unique_ptr<QRhiBuffer>  vbuf;
    std::unique_ptr<QRhiBuffer>  ubuf;
    std::unique_ptr<QRhiSampler> sampler;
    std::unique_ptr<QRhiShaderResourceBindings> srb;
    std::unique_ptr<QRhiGraphicsPipeline> pipeline;

    QShader vs, fs;
    bool initialized   = false;
    bool pipelineReady = false;

    void bindTextures(QRhiTexture* from, QRhiTexture* to)
    {
        QList<QRhiShaderResourceBinding> bindings;
        bindings.append(QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            ubuf.get()));
        bindings.append(QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage, from, sampler.get()));
        bindings.append(QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage, to ? to : from, sampler.get()));
        srb->setBindings(bindings.cbegin(), bindings.cend());
        srb->create();
    }
};

TransitionPass::TransitionPass() : impl_(std::make_unique<Impl>()) {}

TransitionPass::~TransitionPass()
{
    releaseResources();
}

void TransitionPass::releaseResources()
{
    if (!impl_)
        return;
    impl_->pipeline.reset();
    impl_->srb.reset();
    impl_->ubuf.reset();
    impl_->vbuf.reset();
    impl_->sampler.reset();
    impl_->initialized   = false;
    impl_->pipelineReady = false;
}

bool TransitionPass::initialize(void* rhiPtr, const QByteArray& qsbVert, const QByteArray& qsbFrag)
{
    auto* rhi = static_cast<QRhi*>(rhiPtr);
    impl_->rhi = rhi;

    impl_->vs = QShader::fromSerialized(qsbVert);
    impl_->fs = QShader::fromSerialized(qsbFrag);
    if (!impl_->vs.isValid() || !impl_->fs.isValid()) {
        qCWarning(lcRender) << "Invalid shader blobs for transition pass";
        return false;
    }

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
                                     sizeof(TransitionUniforms)));
    if (!impl_->ubuf || !impl_->ubuf->create())
        return false;

    impl_->sampler.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                         QRhiSampler::None, QRhiSampler::ClampToEdge,
                                         QRhiSampler::ClampToEdge));
    if (!impl_->sampler || !impl_->sampler->create())
        return false;

    impl_->srb.reset(rhi->newShaderResourceBindings());
    impl_->bindTextures(nullptr, nullptr);

    impl_->initialized = true;
    return true;
}

void TransitionPass::blend(void* commandBufferPtr, void* renderTargetPtr,
                           void* fromTexturePtr, void* toTexturePtr, const TransitionRef& ref)
{
    auto* cb   = static_cast<QRhiCommandBuffer*>(commandBufferPtr);
    auto* rt   = static_cast<QRhiRenderTarget*>(renderTargetPtr);
    auto* from = static_cast<QRhiTexture*>(fromTexturePtr);
    auto* to   = static_cast<QRhiTexture*>(toTexturePtr);

    if (!impl_->initialized || !cb || !rt || !from)
        return;

    if (!impl_->pipelineReady) {
        impl_->pipeline.reset(impl_->rhi->newGraphicsPipeline());
        impl_->pipeline->setShaderStages({ { QRhiShaderStage::Vertex, impl_->vs },
                                           { QRhiShaderStage::Fragment, impl_->fs } });
        impl_->pipeline->setCullMode(QRhiGraphicsPipeline::None);
        impl_->pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        impl_->pipeline->setSampleCount(1);

        QRhiVertexInputLayout layout;
        layout.setBindings({ { 2 * sizeof(float) } });
        layout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float2, 0 } });
        impl_->pipeline->setVertexInputLayout(layout);
        impl_->pipeline->setShaderResourceBindings(impl_->srb.get());
        impl_->pipeline->setRenderPassDescriptor(rt->renderPassDescriptor());
        impl_->pipelineReady = impl_->pipeline->create();
        if (!impl_->pipelineReady) {
            qCWarning(lcRender) << "TransitionPass: failed to create pipeline";
            return;
        }
    }

    TransitionUniforms u;
    u.params    = QVector4D(ref.params[0], ref.params[1], ref.params[2], ref.params[3]);
    u.fillColor = QVector4D(ref.fillColor[0], ref.fillColor[1],
                            ref.fillColor[2], ref.fillColor[3]);
    u.progress  = ref.progress;
    u.mode      = ref.shaderMode;

    QRhiResourceUpdateBatch* batch = impl_->rhi->nextResourceUpdateBatch();
    batch->updateDynamicBuffer(impl_->ubuf.get(), 0, sizeof(u), &u);

    impl_->bindTextures(from, to);

    const QSize size = rt->pixelSize();
    cb->beginPass(rt, QColor::fromRgbF(0.0f, 0.0f, 0.0f, 0.0f),
                  QRhiDepthStencilClearValue(1.0f, 0), batch);
    cb->setGraphicsPipeline(impl_->pipeline.get());
    cb->setViewport(QRhiViewport(0, 0, float(size.width()), float(size.height())));
    cb->setShaderResources(impl_->srb.get());
    const QRhiCommandBuffer::VertexInput vb(impl_->vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vb);
    cb->draw(3);
    cb->endPass();
}

} // namespace yave::render
