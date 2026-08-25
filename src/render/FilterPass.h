#pragma once

#include "../core/RenderSnapshot.h"

#include <QByteArray>

#include <memory>

namespace yave::render {

/// クリップのビデオフィルタを 1 段ずつ適用する (3.9)。
///
/// スクラッチ RT の ping-pong は呼び出し側 (RhiCompositor) が管理する。
/// このクラスは「1 テクスチャ入力 -> 1 RT 出力」だけを担当する。
///
/// QRhi 型は実装側でのみ触る (ヘッダでは void*)。LayerPass と同じ方針。
class FilterPass
{
public:
    FilterPass();
    ~FilterPass();

    /// color / blur の 2 本のフラグメントシェーダを受け取る。
    bool initialize(void* rhi, const QByteArray& qsbVert,
                    const QByteArray& qsbFragColor, const QByteArray& qsbFragBlur);

    void releaseResources();

    /// このフィルタが必要とする描画回数 (blur は水平 / 垂直の 2 回)。
    static int subPassCount(const ResolvedFilter& filter);

    /// 1 サブパス分を描く。
    /// srcTexture を読み、renderTarget へ書く。
    void draw(void* commandBuffer, void* renderTarget, void* srcTexture,
              const ResolvedFilter& filter, int subPass);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace yave::render
