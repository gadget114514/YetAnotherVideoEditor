#pragma once

#include <atomic>
#include <cstdint>

namespace yave::audio {

/// SPSC ロックフリーリングバッファ。
///
/// 用途: Audio RT Thread -> UI Thread のメーター値転送など、
/// 単一生産者 / 単一消費者の小さなデータ受け渡し。
///
/// 制約: capacity は 2 の冪であること (ビットマスクで剰余を回避するため)。
template <typename T>
class LockFreeRingBuffer
{
public:
    explicit LockFreeRingBuffer(size_t capacityPow2)
        : capacity_(capacityPow2),
          mask_(capacityPow2 - 1)
    {
        // capacity を 2 の冪に切り上げる
        size_t cap = 1;
        while (cap < capacityPow2)
            cap <<= 1;
        capacity_ = cap;
        mask_     = cap - 1;
        buffer_   = new T[cap];
    }

    ~LockFreeRingBuffer() { delete[] buffer_; }

    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;

    bool push(const T& value) noexcept
    {
        const size_t write = writePos_.load(std::memory_order_relaxed);
        if (write - readPos_.load(std::memory_order_acquire) >= capacity_)
            return false;                       ///< full
        buffer_[write & mask_] = value;
        writePos_.store(write + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& out) noexcept
    {
        const size_t read = readPos_.load(std::memory_order_relaxed);
        if (read >= writePos_.load(std::memory_order_acquire))
            return false;                       ///< empty
        out = buffer_[read & mask_];
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
    T*                buffer_ = nullptr;
    size_t            capacity_;
    size_t            mask_;
    alignas(64) std::atomic<size_t> writePos_{0};   ///< 生産者 / 消費者で別キャッシュライン
    alignas(64) std::atomic<size_t> readPos_{0};
};

} // namespace yave::audio
