#pragma once

#include "VideoDecoder.h"

#include <QHash>
#include <QMutex>
#include <QSize>
#include <QUuid>

#include <cstdint>
#include <functional>
#include <memory>

namespace yave::media {

/// デコード済みフレーム 1 枚分。
struct CachedFrame
{
    QByteArray pixels;      ///< RGBA8 premultiplied
    QSize      size;
    int64_t    sourceFrame = 0;
    quint32    lruStamp = 0;
};

/// LRU デコード済みフレームキャッシュ (3.5 / 4 章参照)。
///
/// 予算超過時は LRU で解放する。4K RGBA8 = 33MB/枚なので、
/// 既定予算 512MB で約 15 枚。長尺シークでは即座に溢れるため、
/// 「再生ヘッド近傍のみ」を保持する用途が主。
class FrameCache
{
public:
    explicit FrameCache(qint64 budgetBytes = 512LL * 1024 * 1024);

    /// キャッシュから取得。ヒットしたら lru を更新する。
    std::shared_ptr<CachedFrame> get(const QUuid& assetId, int64_t sourceFrame);

    void put(const QUuid& assetId, int64_t sourceFrame,
             const QByteArray& pixels, const QSize& size);

    /// 使われていないフレームを解放する。メモリ上限維持のため定期呼び出し推奨。
    void trim();

    qint64 usedBytes() const { return usedBytes_; }

private:
    struct Key
    {
        QUuid   assetId;
        int64_t frame;
        bool operator==(const Key& o) const
        { return assetId == o.assetId && frame == o.frame; }
    };
    friend size_t qHash(const Key& k, size_t seed = 0) noexcept
    {
        return ::qHash(k.assetId, seed) ^ std::hash<int64_t>{}(k.frame);
    }

    qint64 budgetBytes_;
    qint64 usedBytes_ = 0;
    quint32 stamp_ = 0;

    QMutex mutex_;
    QHash<Key, std::shared_ptr<CachedFrame>> entries_;
};

} // namespace yave::media
