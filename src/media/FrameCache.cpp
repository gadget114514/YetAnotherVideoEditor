#include "FrameCache.h"

#include <QReadLocker>

#include <algorithm>
#include <functional>

namespace yave::media {

FrameCache::FrameCache(qint64 budgetBytes)
    : budgetBytes_(budgetBytes)
{}

std::shared_ptr<CachedFrame> FrameCache::get(const QUuid& assetId, int64_t sourceFrame)
{
    QMutexLocker lock(&mutex_);
    auto it = entries_.find(Key{assetId, sourceFrame});
    if (it == entries_.end())
        return nullptr;
    it.value()->lruStamp = ++stamp_;
    return it.value();
}

void FrameCache::put(const QUuid& assetId, int64_t sourceFrame,
                     const QByteArray& pixels, const QSize& size)
{
    {
        QMutexLocker lock(&mutex_);
        entries_.insert(Key{assetId, sourceFrame},
                        std::make_shared<CachedFrame>(
                            CachedFrame{pixels, size, sourceFrame, ++stamp_}));
        usedBytes_ += pixels.size();
    }
    trim();
}

void FrameCache::trim()
{
    // 予算内に収まるまで、LRU の末尾 (stamp が古いもの) から解放する。
    QMutexLocker lock(&mutex_);
    while (usedBytes_ > budgetBytes_ && !entries_.empty()) {
        auto oldestIt = entries_.begin();
        quint32 oldest = ~0u;
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it.value()->lruStamp < oldest) {
                oldest   = it.value()->lruStamp;
                oldestIt = it;
            }
        }
        usedBytes_ -= oldestIt.value()->pixels.size();
        entries_.erase(oldestIt);
    }
}

} // namespace yave::media
