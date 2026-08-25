#pragma once

#include "../core/RenderSnapshot.h"

#include <QByteArray>

#include <memory>

namespace yave::render {

/// クリップ境界のトランジションを 1 枚に潰す (3.10)。
///
/// 入力は「対になる 2 レイヤー」のテクスチャ。出力は 1 枚。
/// 潰した結果を LayerPass が通常のレイヤーとして背面へ重ねる。
class TransitionPass
{
public:
    TransitionPass();
    ~TransitionPass();

    bool initialize(void* rhi, const QByteArray& qsbVert, const QByteArray& qsbFrag);
    void releaseResources();

    /// fromTexture / toTexture を ref に従って合成し、renderTarget へ描く。
    /// toTexture が null の場合 (端の境界) は fromTexture を両方に使う。
    void blend(void* commandBuffer, void* renderTarget,
               void* fromTexture, void* toTexture, const TransitionRef& ref);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace yave::render
