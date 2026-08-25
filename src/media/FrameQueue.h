#pragma once

#include <QByteArray>

#include <atomic>
#include <cstdint>

namespace yave::media {

/// SPSC ロックフリーリングバッファ (Decode Worker -> Render Thread のフレーム転送)。
///
/// LockFreeRingBuffer と同様の設計だが、フレーム (バイト列 + メタ) を
/// move で受け渡す点が異なる。capacity は 2 の冪に切り上げられる。
class FrameQueue
{
public:
    struct Item
    {
        QByteArray pixels;
        int        width = 0;
        int        height = 0;
        int64_t    sourceFrame = 0;
        bool       endOfStream = false;
    };

    explicit FrameQueue(size_t capacityPow2 = 16)
    {
        size_t cap = 1;
        while (cap < capacityPow2)
            cap <<= 1;
        capacity_ = cap;
        mask_     = cap - 1;
        slots_    = new Slot[cap];
    }

    ~FrameQueue() { delete[] slots_; }

    FrameQueue(const FrameQueue&) = delete;
    FrameQueue& operator=(const FrameQueue&) = delete;

    bool push(Item&& item) noexcept
    {
        const size_t write = writePos_.load(std::memory_order_relaxed);
        if (write - readPos_.load(std::memory_order_acquire) >= capacity_)
            return false;                       ///< full: デコーダは待機 / 破棄を選ぶ
        slots_[write & mask_].item = std::move(item);
        writePos_.store(write + 1, std::memory_order_release);
        return true;
    }

    bool pop(Item& out) noexcept
    {
        const size_t read = readPos_.load(std::memory_order_relaxed);
        if (read >= writePos_.load(std::memory_order_acquire))
            return false;                       ///< empty
        out = std::move(slots_[read & mask_].item);
        readPos_.store(read + 1, std::memory_order_release);
        return true;
    }

    size_t size() const noexcept
    {
        return writePos_.load(std::memory_order_acquire)
             - readPos_.load(std::memory_order_acquire);
    }
    size_t capacity() const noexcept { return capacity_; }

private:
    struct Slot { Item item; };

    Slot*  slots_ = nullptr;
    size_t capacity_;
    size_t mask_;
    alignas(64) std::atomic<size_t> writePos_{0};
    alignas(64) std::atomic<size_t> readPos_{0};
};

} // namespace yave::media
