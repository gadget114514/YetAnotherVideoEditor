#include "TexturePool.h"

#include "../util/Log.h"

#include <rhi/qrhi.h>

namespace yave::render {

struct TexturePool::Entry
{
    std::unique_ptr<QRhiTexture> tex;
    int  lastUsedFrame = 0;
    bool inUse = false;
};

TexturePool::TexturePool(void* rhi, qint64 budgetBytes)
    : rhi_(rhi), budgetBytes_(budgetBytes)
{}

TexturePool::~TexturePool()
{
    // QRhiTexture の解放は QRhi より先に行う必要があるため、
    // デストラクタの順序に注意 (呼び出し側が RhiContext 破棄前に破棄すること)。
    pool_.clear();
}

static qint64 textureBytes(const TexturePool::Key& k)
{
    // RGBA8 基準。他フォーマットは係数で概算する。
    constexpr qint64 bytesPerPx = 4;
    return qint64(k.size.width()) * qint64(k.size.height()) * bytesPerPx;
}

void* TexturePool::acquire(const Key& key)
{
    if (!rhi_)
        return nullptr;

    auto range = pool_.equal_range(std::hash<Key>{}(key));
    for (auto it = range.first; it != range.second; ++it) {
        Entry& e = *it->second;
        if (!e.inUse && e.tex && e.tex->pixelSize() == key.size) {
            e.inUse         = true;
            e.lastUsedFrame = currentFrame_;
            return e.tex.get();
        }
    }

    // 新規生成
    const qint64 cost = textureBytes(key);
    if (usedBytes_ + cost > budgetBytes_) {
        trim(0);
        if (usedBytes_ + cost > budgetBytes_) {
            qCWarning(lcRender) << "TexturePool budget exceeded;"
                                << "skipping layer render"
                                << key.size.width() << "x" << key.size.height();
            return nullptr;   ///< クラッシュさせず描画をスキップ (3.5)
        }
    }

    auto entry = std::make_unique<Entry>();
    auto* rhi = static_cast<QRhi*>(rhi_);
    entry->tex.reset(rhi->newTexture(QRhiTexture::RGBA8, key.size, 1,
                                     QRhiTexture::UsedAsTransferSource));
    if (!entry->tex || !entry->tex->create()) {
        return nullptr;
    }
    entry->inUse         = true;
    entry->lastUsedFrame = currentFrame_;
    usedBytes_ += cost;

    void* raw = entry->tex.get();
    pool_.emplace(std::hash<Key>{}(key), std::move(entry));
    return raw;
}

void TexturePool::release(void* texture)
{
    if (!texture)
        return;
    for (auto& [hash, entry] : pool_) {
        if (entry->tex.get() == texture) {
            entry->inUse = false;
            entry->lastUsedFrame = currentFrame_;
            return;
        }
    }
}

void TexturePool::trim(int unusedFrameThreshold)
{
    ++currentFrame_;
    const int threshold = currentFrame_ - unusedFrameThreshold;

    for (auto it = pool_.begin(); it != pool_.end();) {
        Entry& e = *it->second;
        if (!e.inUse && e.lastUsedFrame < threshold) {
            usedBytes_ -= qint64(e.tex->pixelSize().width())
                          * qint64(e.tex->pixelSize().height()) * 4;
            it = pool_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace yave::render
