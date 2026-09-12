#pragma once

#include "../core/RenderSnapshot.h"

#include <QSize>

#include <functional>
#include <memory>

namespace yave::render {

/// レイヤー合成の中核 (3.4 合成パイプライン)。
///
/// Render Thread から renderFrame(snapshot) を呼ぶ。
/// Timeline は直接参照せず、RenderSnapshot (値のコピー) 越しにのみ参照する。
///
/// オフスクリーン RT を挟む理由:
///   - 出力解像度 (4K) とプレビュー解像度を分離できる。字幕は必ず出力解像度で
///     ラスタライズし、最後に一度だけ縮小する。
///   - 書き出しが同じ合成コードを再利用できる。
///
/// ping-pong: dstTex を読むブレンドモードがあるため、compositeRtA/B の 2 枚を
/// 交互に使う。Normal ブレンドのみの連続は固定機能ブレンドへ切り替えて省略できる。
class RhiCompositor
{
public:
    RhiCompositor();
    ~RhiCompositor();

    /// QRhi への接続 (QQuickRhiItem の initialize() から呼ばれる)。
    void initialize(void* rhi, void* swapchainFormat);

    void releaseResources();

    /// スナップショットを合成し、結果テクスチャを返す (void* = QRhiTexture*)。
    /// commandBuffer は PreviewItem の render() に渡される、Scene Graph の
    /// フレームで使用中のコマンドバッファ。フレームの入れ子は作らない。
    void* renderFrame(void* commandBuffer, const RenderSnapshot& snapshot);

    /// 合成結果テクスチャを、表示用レンダーターゲット (PreviewItem の
    /// カラーテクスチャ / renderTarget()) へ描画する (3.4.1 PRESENT)。
    /// アスペクトは維持せず、ターゲット全体へ拡縮描画する。
    void present(void* commandBuffer, void* renderTarget, void* compositeTexture);

    /// AI 参照フレーム要求の処理。trackId + frame を合成して outputPath へ PNG 保存する。
    /// ReferenceFrameSink 経由で ai モジュールから呼ばれる。
    static void requestFrameRender(const QUuid& trackId, int64_t frame,
                                   const QString& outputPath);

    /// FrameCache への接続 (yave_media の FrameCache*)。
    /// void* で受け、実ファイルでは reinterpret_cast する (ヘッダ依存回避)。
    void setFrameCache(void* cache) { frameCache_ = cache; }

    /// 出力解像度の変更 (RT 再生成が必要)
    void setOutputSize(const QSize& size);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    void* frameCache_ = nullptr;   ///< yave::media::FrameCache*
};

} // namespace yave::render
