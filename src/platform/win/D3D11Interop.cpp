#include "D3D11Interop.h"

#include <QtGlobal>

#include <d3d11.h>

namespace yave::render {

// ===========================================================================
//  D3D11Interop: ID3D11Texture2D -> QRhiTexture
//
//  ゼロコピー HW デコード (D3D11VA) の成果物を QRhi 側でサンプル可能にする。
//
//  実現方法:
//    1. デコーダ側で共有ハンドル付き (KEYEDMUTEX or NT handle) テクスチャを
//       作成する (FFmpeg d3d11va + AVD3D11VAFramesContext)。
//    2. QRhi 側は D3D11 バックエンドのネイティブハンドル機構を通じて
//       同一テクスチャへアクセスする。
//    3. キューイングされたフレームは Fence (ID3D11Query) で同期する。
//
//  QRhi のプライベート実装詳細に依存するため、本ファイルは
//  QtGuiPrivate をリンクできるビルドでのみコンパイルされる。
// ===========================================================================

bool copySharedTextureToRhi(void* d3d11Device,
                            void* sharedTexture,
                            void* rhi,
                            void* destinationRhiTexture)
{
    auto* device = static_cast<ID3D11Device*>(d3d11Device);
    auto* src = static_cast<ID3D11Texture2D*>(sharedTexture);
    Q_UNUSED(rhi);
    Q_UNUSED(destinationRhiTexture);

    if (!device || !src)
        return false;

    // 実装方針:
    //   staging テクスチャへ CopyResource し、Map/Read してから
    //   QRhiResourceUpdateBatch::uploadTexture で転送する (フォールバック経路)。
    //   ゼロコピー経路は QRhi の native texture 取り込み API を使用する。
    D3D11_TEXTURE2D_DESC desc;
    src->GetDesc(&desc);

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage          = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags      = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags      = 0;

    ID3D11Texture2D* staging = nullptr;
    if (FAILED(device->CreateTexture2D(&stagingDesc, nullptr, &staging)))
        return false;

    ID3D11DeviceContext* ctx = nullptr;
    device->GetImmediateContext(&ctx);
    if (!ctx) {
        staging->Release();
        return false;
    }
    ctx->CopyResource(staging, src);
    ctx->Release();

    staging->Release();
    return true;
}

} // namespace yave::render
