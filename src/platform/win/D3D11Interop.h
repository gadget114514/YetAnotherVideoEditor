#pragma once

namespace yave::render {

/// D3D11 共有テクスチャ -> QRhiTexture 転送 (ゼロコピーのフォールバック経路)。
/// 実装は D3D11Interop.cpp。Windows ビルドでのみコンパイルされる。
bool copySharedTextureToRhi(void* d3d11Device,
                            void* sharedTexture,
                            void* rhi,
                            void* destinationRhiTexture);

} // namespace yave::render
