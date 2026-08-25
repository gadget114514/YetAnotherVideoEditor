#pragma once

#include "../core/RenderSnapshot.h"

#include <QMatrix4x4>

#include <memory>

namespace yave::render {

/// 1 レイヤー分の描画パス (3.4.1 COMPOSITE 参照)。
///
/// 頂点: フルスクリーン三角形 (2 頂点キャッシュ)
/// 変換とブレンドモードは uniform で渡す
/// YUV テクスチャならフラグメントで RGB 変換
///
/// QRhi 型は実装側でのみ触る (ヘッダでは void*)。
class LayerPass
{
public:
    LayerPass();
    ~LayerPass();

    /// パイプラインの初期化。rhi とレンダーターゲットの形式が必要。
    bool initialize(void* rhi, void* renderTargetFormat, const QByteArray& qsbVert,
                    const QByteArray& qsbFrag);

    /// リソース解放。QRhi 破棄より先に呼ぶこと。
    void releaseResources();

    /// 1 レイヤーを compositeTarget へ描画する。
    ///
    /// srcTexture : 上に乗せるレイヤー
    /// dstTexture : ここまでの合成結果 (ping-pong)
    void draw(void* commandBuffer,
              void* renderPassDescriptor,
              void* srcTexture,
              void* dstTexture,
              const LayerItem& layer);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace yave::render
