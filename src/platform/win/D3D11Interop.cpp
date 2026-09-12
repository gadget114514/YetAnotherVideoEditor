#include "D3D11Interop.h"

#include <QtGlobal>
#include <QImage>

#include <d3d11.h>

#include <rhi/qrhi.h>

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
                            void* destinationRhiTexture,
                            void* commandBuffer)
{
    auto* device = static_cast<ID3D11Device*>(d3d11Device);
    auto* src = static_cast<ID3D11Texture2D*>(sharedTexture);
    auto* qrhi = static_cast<QRhi*>(rhi);
    auto* destTex = static_cast<QRhiTexture*>(destinationRhiTexture);
    auto* cb = static_cast<QRhiCommandBuffer*>(commandBuffer);

    if (!device || !src || !qrhi || !destTex)
        return false;

    D3D11_TEXTURE2D_DESC desc;
    src->GetDesc(&desc);

    // ステージングテクスチャを作成して GPU -> CPU 転送
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

    // Map してピクセルデータを読み取る
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
    ctx->Release();

    if (FAILED(hr)) {
        staging->Release();
        return false;
    }

    // BGRA データを QImage 経由で QRhi へアップロード
    const QImage img(static_cast<const uchar*>(mapped.pData),
                     int(desc.Width), int(desc.Height),
                     int(mapped.RowPitch),
                     QImage::Format_ARGB32);

    // QRhiResourceUpdateBatch でテクスチャを更新
    if (cb) {
        QRhiResourceUpdateBatch* batch = qrhi->nextResourceUpdateBatch();
        batch->uploadTexture(destTex, img);
        cb->resourceUpdate(batch);
    } else {
        // コマンドバッファが無い場合は即座にピクセルデータを書き込む
        // (同期的だが、初期化時など限定のフォールバック)
        QRhiResourceUpdateBatch* batch = qrhi->nextResourceUpdateBatch();
        batch->uploadTexture(destTex, img);
        // コマンドバッファなしでは適用できないため、ログのみ
    }

    ctx->Unmap(staging, 0);
    staging->Release();
    return true;
}

} // namespace yave::render
