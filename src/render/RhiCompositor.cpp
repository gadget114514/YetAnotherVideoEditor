#include "RhiCompositor.h"
#include "FilterPass.h"
#include "LayerPass.h"
#include "TexturePool.h"
#include "TransitionPass.h"

#include "../media/FrameCache.h"
#include "../util/Log.h"

#include <QFile>
#include <QImage>
#include <QMutex>
#include <QSaveFile>
#include <QVector>

#include <rhi/qrhi.h>

namespace yave::render {

// ===========================================================================
//  AI 参照フレーム要求の受け口
//
//  ai モジュールは yave_render へ依存できないため、関数のブリッジで
//  受け、app 層が実装 (RhiCompositor インスタンスへの転送) を注入する。
// ===========================================================================

namespace {

struct FrameRenderRequest
{
    QUuid   trackId;
    int64_t frame = 0;
    QString outputPath;
};

QMutex g_requestMutex;
std::vector<FrameRenderRequest> g_pendingRequests;

} // anonymous namespace

void RhiCompositor::requestFrameRender(const QUuid& trackId, int64_t frame,
                                       const QString& outputPath)
{
    QMutexLocker lock(&g_requestMutex);
    g_pendingRequests.push_back({trackId, frame, outputPath});
}

// layer_blend.frag の uniform ブロックと一致させる (3.4.3)。PRESENT 用。
struct alignas(16) PresentUniforms
{
    QMatrix4x4 transform;
    QVector4D  cropRect;      // x, y, w, h  (0..1)
    float      opacity;
    int        blendMode;
    int        colorSpace;
    float      pad;
};

struct RhiCompositor::Impl
{
    QRhi* rhi = nullptr;

    std::unique_ptr<LayerPass> layerPass;
    std::unique_ptr<FilterPass> filterPass;
    std::unique_ptr<TransitionPass> transitionPass;
    std::unique_ptr<TexturePool> texturePool;

    // ping-pong 用オフスクリーン RT
    std::unique_ptr<QRhiTexture> rtTex[2];
    std::unique_ptr<QRhiTextureRenderTarget> renderTarget[2];
    std::unique_ptr<QRhiRenderPassDescriptor> rpDesc[2];
    // フィルタ / トランジションの中間結果用 (3.9 / 3.10)。合成 RT とは別に持つ。
    std::unique_ptr<QRhiTexture> scratchTex[2];
    std::unique_ptr<QRhiTextureRenderTarget> scratchRt[2];
    std::unique_ptr<QRhiRenderPassDescriptor> scratchRpDesc[2];

    QSize outputSize{1920, 1080};
    int   currentTarget = 0;

    bool initialized = false;

    void ensureRenderTargets()
    {
        if (!rhi)
            return;

        bool needRecreate = false;
        for (int i = 0; i < 2; ++i) {
            if (!rtTex[i] || rtTex[i]->pixelSize() != outputSize) {
                needRecreate = true;
                break;
            }
        }
        if (!needRecreate)
            return;

        for (int i = 0; i < 2; ++i) {
            rtTex[i].reset(rhi->newTexture(QRhiTexture::RGBA8, outputSize, 1,
                                           QRhiTexture::RenderTarget
                                               | QRhiTexture::UsedAsTransferSource));
            if (!rtTex[i] || !rtTex[i]->create()) {
                qCWarning(lcRender) << "Failed to create composite RT" << i;
                rtTex[i].reset();
                renderTarget[i].reset();
                rpDesc[i].reset();
                continue;
            }

            QRhiColorAttachment colorAtt(rtTex[i].get());
            renderTarget[i].reset(rhi->newTextureRenderTarget({colorAtt}));
            rpDesc[i].reset(renderTarget[i]->renderPassDescriptor());
            renderTarget[i]->create();
        }

        // スクラッチも同じサイズ / フォーマットで用意する。
        // 合成 RT と互換な rp 記述子になるので、パイプラインを使い回せる。
        for (int i = 0; i < 2; ++i) {
            scratchTex[i].reset(rhi->newTexture(QRhiTexture::RGBA8, outputSize, 1,
                                                QRhiTexture::RenderTarget));
            if (!scratchTex[i] || !scratchTex[i]->create()) {
                qCWarning(lcRender) << "Failed to create scratch RT" << i;
                scratchTex[i].reset();
                scratchRt[i].reset();
                scratchRpDesc[i].reset();
                continue;
            }
            QRhiColorAttachment att(scratchTex[i].get());
            scratchRt[i].reset(rhi->newTextureRenderTarget({att}));
            scratchRpDesc[i].reset(scratchRt[i]->renderPassDescriptor());
            scratchRt[i]->create();
        }
    }

    /// レイヤーのフィルタスタックを適用し、結果テクスチャを返す (3.9)。
    /// フィルタが無い / 適用できない場合は入力をそのまま返す。
    QRhiTexture* applyFilters(QRhiCommandBuffer* cb, QRhiTexture* src,
                              const std::vector<ResolvedFilter>& filters)
    {
        if (filters.empty() || !filterPass || !scratchRt[0] || !scratchRt[1])
            return src;

        QRhiTexture* current = src;
        int slot = 0;
        for (const auto& f : filters) {
            const int subPasses = FilterPass::subPassCount(f);
            for (int sp = 0; sp < subPasses; ++sp) {
                filterPass->draw(cb, scratchRt[slot].get(), current, f, sp);
                current = scratchTex[slot].get();
                slot = 1 - slot;
            }
        }
        return current;
    }

    // ---- プレビュー表示 (PRESENT) 用の簡易パス ----
    // 合成結果テクスチャを PreviewItem のカラーテクスチャへ描画する。
    // パイプラインはターゲットの renderPassDescriptor に依存するため、
    // 変化したら作り直す。
    std::unique_ptr<QRhiGraphicsPipeline> presentPipeline;
    std::unique_ptr<QRhiShaderResourceBindings> presentSrb;
    std::unique_ptr<QRhiSampler> presentSampler;
    std::unique_ptr<QRhiBuffer> presentVbuf;
    std::unique_ptr<QRhiBuffer> presentUbuf;
    QRhiRenderPassDescriptor* presentRpDesc = nullptr;
    bool presentReady = false;

    QByteArray uvVertData;     ///< fullscreen_uv.vert.qsb
    QByteArray blendFragData;  ///< layer_blend.frag.qsb

    void ensurePresentPipeline(QRhiRenderTarget* rt)
    {
        if (rt->renderPassDescriptor() == presentRpDesc && presentPipeline)
            return;

        presentPipeline.reset();
        presentSrb.reset();

        const QShader vs = QShader::fromSerialized(uvVertData);
        const QShader fs = QShader::fromSerialized(blendFragData);
        if (!vs.isValid() || !fs.isValid()) {
            presentReady = false;
            return;
        }

        if (!presentSampler) {
            presentSampler.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                                 QRhiSampler::None, QRhiSampler::ClampToEdge,
                                                 QRhiSampler::ClampToEdge));
            presentSampler->create();
        }

        // フルスクリーン三角形 (位置 + UV)。Dynamic にして毎フレーム書き込む
        // (initialize 時点ではフレームが無いため静的アップロードを避ける)。
        static const float vertexData[] = {
            -1.0f, -1.0f, 0.0f, 1.0f,
             3.0f, -1.0f, 2.0f, 1.0f,
            -1.0f,  3.0f, 0.0f, -1.0f,
        };
        if (!presentVbuf) {
            presentVbuf.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                             sizeof(vertexData)));
            presentVbuf->create();
        }
        if (!presentUbuf) {
            presentUbuf.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                             sizeof(PresentUniforms)));
            presentUbuf->create();
        }

        presentSrb.reset(rhi->newShaderResourceBindings());
        presentPipeline.reset(rhi->newGraphicsPipeline());
        presentPipeline->setShaderStages({
            { QRhiShaderStage::Vertex, vs },
            { QRhiShaderStage::Fragment, fs },
        });
        presentPipeline->setCullMode(QRhiGraphicsPipeline::None);
        presentPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        presentPipeline->setSampleCount(1);

        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({ { 4 * sizeof(float) } });
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
            { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) },
        });
        presentPipeline->setVertexInputLayout(inputLayout);
        presentPipeline->setShaderResourceBindings(presentSrb.get());
        presentPipeline->setRenderPassDescriptor(rt->renderPassDescriptor());
        presentReady = presentPipeline->create();
        presentRpDesc = rt->renderPassDescriptor();
    }
};

RhiCompositor::RhiCompositor() : impl_(std::make_unique<Impl>()) {}

RhiCompositor::~RhiCompositor()
{
    releaseResources();
}

void RhiCompositor::initialize(void* rhiPtr, void*)
{
    auto* rhi = static_cast<QRhi*>(rhiPtr);
    impl_->rhi = rhi;
    impl_->texturePool = std::make_unique<TexturePool>(rhi);
    impl_->layerPass = std::make_unique<LayerPass>();

    // シェーダは qrc 埋め込み (.qsb) から読む
    QFile vsFile(QStringLiteral(":/shaders/fullscreen.vert.qsb"));
    QFile fsFile(QStringLiteral(":/shaders/layer_blend.frag.qsb"));
    QByteArray vsData, fsData;
    if (vsFile.open(QIODevice::ReadOnly))
        vsData = vsFile.readAll();
    if (fsFile.open(QIODevice::ReadOnly))
        fsData = fsFile.readAll();

    // PRESENT パス用シェーダ (UV 頂点 + ブレンド) を保存しておく
    QFile uvVertFile(QStringLiteral(":/shaders/fullscreen_uv.vert.qsb"));
    if (uvVertFile.open(QIODevice::ReadOnly))
        impl_->uvVertData = uvVertFile.readAll();
    impl_->blendFragData = fsData;

    impl_->initialized = impl_->layerPass->initialize(rhi, nullptr, vsData, fsData);
    if (!impl_->initialized)
        qCWarning(lcRender) << "RhiCompositor: LayerPass initialization failed"
                            << "(.qsb resources missing?)";

    // ---- フィルタ / トランジション (3.9 / 3.10) ----
    const auto readShader = [](const QString& path) {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    };
    const QByteArray uvVert   = readShader(QStringLiteral(":/shaders/fullscreen_uv.vert.qsb"));
    const QByteArray fltColor = readShader(QStringLiteral(":/shaders/filter_color.frag.qsb"));
    const QByteArray fltBlur  = readShader(QStringLiteral(":/shaders/filter_blur.frag.qsb"));
    const QByteArray transFs  = readShader(QStringLiteral(":/shaders/transition.frag.qsb"));

    impl_->filterPass = std::make_unique<FilterPass>();
    if (!impl_->filterPass->initialize(rhi, uvVert, fltColor, fltBlur)) {
        qCWarning(lcRender) << "RhiCompositor: FilterPass initialization failed;"
                            << "clips will render without filters";
        impl_->filterPass.reset();
    }

    impl_->transitionPass = std::make_unique<TransitionPass>();
    if (!impl_->transitionPass->initialize(rhi, uvVert, transFs)) {
        qCWarning(lcRender) << "RhiCompositor: TransitionPass initialization failed;"
                            << "transitions will fall back to a hard cut";
        impl_->transitionPass.reset();
    }
}

void RhiCompositor::releaseResources()
{
    if (!impl_)
        return;
    if (impl_->layerPass)
        impl_->layerPass->releaseResources();
    if (impl_->filterPass)
        impl_->filterPass->releaseResources();
    if (impl_->transitionPass)
        impl_->transitionPass->releaseResources();
    impl_->presentPipeline.reset();
    impl_->presentSrb.reset();
    impl_->presentSampler.reset();
    impl_->presentVbuf.reset();
    impl_->presentUbuf.reset();
    impl_->presentRpDesc = nullptr;
    impl_->presentReady = false;
    for (auto& t : impl_->rtTex)
        t.reset();
    for (auto& t : impl_->scratchTex)
        t.reset();
    for (auto& rp : impl_->scratchRpDesc)
        rp.reset();
    for (auto& rt : impl_->scratchRt)
        rt.reset();
    for (auto& rp : impl_->rpDesc)
        rp.reset();
    for (auto& rt : impl_->renderTarget)
        rt.reset();
    impl_->texturePool.reset();
    impl_->initialized = false;
}

void RhiCompositor::setOutputSize(const QSize& size)
{
    if (size == impl_->outputSize || !size.isValid())
        return;
    impl_->outputSize = size;
    // RT 再生成は次フレームの ensureRenderTargets() で行う
    for (auto& t : impl_->rtTex)
        t.reset();
    for (auto& t : impl_->scratchTex)
        t.reset();
}

void* RhiCompositor::renderFrame(void* commandBuffer, const RenderSnapshot& snapshot)
{
    if (!impl_->initialized || !impl_->rhi || !commandBuffer)
        return nullptr;

    auto* cb = static_cast<QRhiCommandBuffer*>(commandBuffer);

    setOutputSize(snapshot.canvasSize);
    impl_->ensureRenderTargets();

    if (!impl_->renderTarget[0] || !impl_->renderTarget[1])
        return nullptr;

    QRhi* rhi = impl_->rhi;

    // ---- (1) PREPARE: 各レイヤーのソーステクスチャを取得 -----------------
    // VideoSourceRef -> FrameCache からデコード済みフレームを取得し GPU へアップロード
    // SubtitleRenderRef -> SubtitleRenderer (glyph atlas) は未接続
    struct PendingUpload {
        QRhiTexture* tex;
        QByteArray pixels;
        QSize size;
    };
    QVector<PendingUpload> pendingUploads;
    auto* frameCache = reinterpret_cast<yave::media::FrameCache*>(frameCache_);
    QVector<void*> layerTextures(int(snapshot.layers.size()), nullptr);
    for (int i = 0; i < int(snapshot.layers.size()); ++i) {
        const LayerItem& layer = snapshot.layers[size_t(i)];

        // VideoSourceRef の場合、FrameCache からピクセルを取得してテクスチャへアップロード
        if (auto* vsr = std::get_if<VideoSourceRef>(&layer.source)) {
            if (frameCache) {
                auto cachedFrame = frameCache->get(vsr->assetId, vsr->sourceFrameIndex);
                if (cachedFrame && !cachedFrame->pixels.isEmpty()) {
                    TexturePool::Key key;
                    key.size   = cachedFrame->size;
                    key.format = 0;  // RGBA8
                    void* tex = impl_->texturePool->acquire(key);
                    if (tex) {
                        pendingUploads.push_back({
                            static_cast<QRhiTexture*>(tex),
                            cachedFrame->pixels,
                            cachedFrame->size
                        });
                        layerTextures[i] = tex;
                    }
                }
            }
            // FrameCache が未接続 or キャッシュミス時はプレースホルダ
            if (!layerTextures[i]) {
                TexturePool::Key key;
                key.size   = QSize(16, 16);
                key.format = 0;
                layerTextures[i] = impl_->texturePool->acquire(key);
                impl_->texturePool->release(layerTextures[i]);
            }
        } else {
            // VideoSourceRef 以外 (字幕 / AI 生成物) はプレースホルダ
            TexturePool::Key key;
            key.size   = QSize(16, 16);
            key.format = 0;
            layerTextures[i] = impl_->texturePool->acquire(key);
            impl_->texturePool->release(layerTextures[i]);
        }
    }

    // ---- (3) COMPOSITE: 背面 -> 前面 -------------------------------------
    int ping = impl_->currentTarget;
    const int layerCount = int(snapshot.layers.size());

    // レイヤーが無い場合は合成 RT を黒でクリアして返す (未定義領域の表示を防ぐ)
    if (layerCount == 0) {
        QRhiResourceUpdateBatch* batch = rhi->nextResourceUpdateBatch();
        cb->beginPass(impl_->renderTarget[ping].get(), Qt::black,
                      QRhiDepthStencilClearValue(1.0f, 0), batch);
        cb->endPass();
        impl_->currentTarget = ping;
        return impl_->rtTex[ping] ? impl_->rtTex[ping].get() : nullptr;
    }

    // FrameCache から取得したピクセルの GPU アップロードを最初のパスで適用する
    QRhiResourceUpdateBatch* uploadBatch = rhi->nextResourceUpdateBatch();
    for (const auto& up : pendingUploads) {
        QImage img(reinterpret_cast<const uchar*>(up.pixels.constData()),
                   up.size.width(), up.size.height(),
                   up.size.width() * 4, QImage::Format_ARGB32);
        uploadBatch->uploadTexture(up.tex, img);
    }

    for (int i = 0; i < layerCount; ++i) {
        const LayerItem& layer = snapshot.layers[size_t(i)];
        if (!layerTextures[i])
            continue;                       ///< テクスチャ確保失敗 -> スキップ

        auto* srcTex = static_cast<QRhiTexture*>(layerTextures[i]);

        // --- トランジションの対 (3.10) ---
        // buildSnapshot() は同じ zIndex の 2 レイヤーを from, to の順で連続して積む。
        // ここで 1 枚に潰し、以降は通常のレイヤーとして扱う。
        const LayerItem* composited = &layer;
        if (layer.transition && impl_->transitionPass && i + 1 < layerCount) {
            const LayerItem& next = snapshot.layers[size_t(i) + 1];
            if (next.transition && next.zIndex == layer.zIndex && layerTextures[i + 1]) {
                auto* fromTex = impl_->applyFilters(cb, srcTex, layer.filters);
                auto* toTex   = impl_->applyFilters(
                    cb, static_cast<QRhiTexture*>(layerTextures[i + 1]), next.filters);

                if (impl_->scratchRt[0]) {
                    impl_->transitionPass->blend(cb, impl_->scratchRt[0].get(),
                                                 fromTex, toTex, *layer.transition);
                    srcTex = impl_->scratchTex[0].get();
                }
                ++i;                        ///< 対の 2 枚目は処理済み
                const int pong = 1 - ping;
                void* preBatch = (uploadBatch) ? uploadBatch : nullptr;
                impl_->layerPass->draw(cb, impl_->renderTarget[pong].get(),
                                       srcTex, impl_->rtTex[ping].get(), *composited,
                                       preBatch);
                if (uploadBatch)
                    uploadBatch = nullptr;
                ping = pong;
                continue;
            }
        }

        // --- 通常のレイヤー: フィルタ -> 合成 (3.9) ---
        srcTex = impl_->applyFilters(cb, srcTex, layer.filters);

        const int pong = 1 - ping;
        // 最初のレイヤーに限り、FrameCache からのテクスチャアップロードバッチを適用
        void* preBatch = (i == 0 && uploadBatch) ? uploadBatch : nullptr;
        impl_->layerPass->draw(cb,
                               impl_->renderTarget[pong].get(),
                               srcTex,
                               impl_->rtTex[ping].get(), *composited, preBatch);
        if (i == 0)
            uploadBatch = nullptr;   ///< 適用済み
        ping = pong;
    }
    impl_->currentTarget = ping;

    return impl_->rtTex[ping] ? impl_->rtTex[ping].get() : nullptr;
}

void RhiCompositor::present(void* commandBufferPtr, void* renderTargetPtr,
                            void* compositePtr)
{
    if (!impl_ || !impl_->rhi || !commandBufferPtr || !renderTargetPtr || !compositePtr)
        return;

    auto* cb         = static_cast<QRhiCommandBuffer*>(commandBufferPtr);
    auto* rt         = static_cast<QRhiRenderTarget*>(renderTargetPtr);
    auto* composite  = static_cast<QRhiTexture*>(compositePtr);

    if (impl_->uvVertData.isEmpty() || impl_->blendFragData.isEmpty())
        return;
    impl_->ensurePresentPipeline(rt);
    if (!impl_->presentReady || !impl_->presentPipeline)
        return;

    PresentUniforms u;
    u.transform.setToIdentity();
    u.cropRect  = QVector4D(0.0f, 0.0f, 1.0f, 1.0f);
    u.opacity   = 1.0f;
    u.blendMode = 0;    ///< Normal
    u.colorSpace = 0;
    u.pad        = 0.0f;

    static const float vertexData[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         3.0f, -1.0f, 2.0f, 1.0f,
        -1.0f,  3.0f, 0.0f, -1.0f,
    };

    QList<QRhiShaderResourceBinding> bindings;
    bindings.append(QRhiShaderResourceBinding::uniformBuffer(
        0, QRhiShaderResourceBinding::FragmentStage, impl_->presentUbuf.get()));
    bindings.append(QRhiShaderResourceBinding::sampledTexture(
        1, QRhiShaderResourceBinding::FragmentStage, composite, impl_->presentSampler.get()));
    bindings.append(QRhiShaderResourceBinding::sampledTexture(
        2, QRhiShaderResourceBinding::FragmentStage, composite, impl_->presentSampler.get()));
    impl_->presentSrb->setBindings(bindings.cbegin(), bindings.cend());
    impl_->presentSrb->create();

    QRhiResourceUpdateBatch* batch = impl_->rhi->nextResourceUpdateBatch();
    batch->updateDynamicBuffer(impl_->presentVbuf.get(), 0, sizeof(vertexData), vertexData);
    batch->updateDynamicBuffer(impl_->presentUbuf.get(), 0, sizeof(u), &u);

    cb->beginPass(rt, Qt::black, QRhiDepthStencilClearValue(1.0f, 0), batch);
    cb->setGraphicsPipeline(impl_->presentPipeline.get());
    cb->setViewport(QRhiViewport(0, 0, float(rt->pixelSize().width()),
                                 float(rt->pixelSize().height())));
    cb->setShaderResources(impl_->presentSrb.get());
    const QRhiCommandBuffer::VertexInput vb(impl_->presentVbuf.get(), 0);
    cb->setVertexInput(0, 1, &vb);
    cb->draw(3);
    cb->endPass();
}

} // namespace yave::render
