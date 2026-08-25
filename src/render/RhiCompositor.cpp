#include "RhiCompositor.h"
#include "FilterPass.h"
#include "LayerPass.h"
#include "TexturePool.h"
#include "TransitionPass.h"

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

void* RhiCompositor::renderFrame(const RenderSnapshot& snapshot)
{
    if (!impl_->initialized || !impl_->rhi)
        return nullptr;

    setOutputSize(snapshot.canvasSize);
    impl_->ensureRenderTargets();

    if (!impl_->renderTarget[0] || !impl_->renderTarget[1])
        return nullptr;

    QRhi* rhi = impl_->rhi;
    QRhiCommandBuffer* cb = nullptr;
    if (!rhi->beginOffscreenFrame(&cb))
        return nullptr;

    // ---- (1) PREPARE: 各レイヤーのソーステクスチャを取得 -----------------
    // VideoSourceRef -> FrameCache / DecodeWorkerPool
    // SubtitleRenderRef -> SubtitleRenderer (glyph atlas)
    // 現行ビルドではソース供給が未接続のため、小さなプレースホルダを割り当てる。
    QVector<void*> layerTextures(int(snapshot.layers.size()), nullptr);
    for (int i = 0; i < int(snapshot.layers.size()); ++i) {
        TexturePool::Key key;
        key.size   = QSize(16, 16);   ///< プレースホルダ
        key.format = 0;
        layerTextures[i] = impl_->texturePool->acquire(key);
        impl_->texturePool->release(layerTextures[i]);   ///< 今フレームのみ使用
    }

    // ---- (3) COMPOSITE: 背面 -> 前面 -------------------------------------
    int ping = impl_->currentTarget;
    const int layerCount = int(snapshot.layers.size());
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
                impl_->layerPass->draw(cb, impl_->renderTarget[pong].get(),
                                       srcTex, impl_->rtTex[ping].get(), *composited);
                ping = pong;
                continue;
            }
        }

        // --- 通常のレイヤー: フィルタ -> 合成 (3.9) ---
        srcTex = impl_->applyFilters(cb, srcTex, layer.filters);

        const int pong = 1 - ping;
        impl_->layerPass->draw(cb,
                               impl_->renderTarget[pong].get(),
                               srcTex,
                               impl_->rtTex[ping].get(), *composited);
        ping = pong;
    }
    impl_->currentTarget = ping;

    // ---- (5) STATS: GPU タイムスタンプ収集は PerfMonitor へ (将来拡張) ----
    rhi->endOffscreenFrame();

    return impl_->rtTex[ping] ? impl_->rtTex[ping].get() : nullptr;
}

} // namespace yave::render
