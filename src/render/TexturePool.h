#pragma once

#include <QHash>
#include <QSize>
#include <QFlags>

#include <unordered_map>
#include <memory>

namespace yave::render {

/// 4K RGBA8 テクスチャは 32MB。毎フレーム生成/破棄すると GPU メモリの断片化と
/// スタッタが起きるため、テクスチャをプールして再利用する (3.5 参照)。
///
/// QRhi への依存は実装ファイル側に閉じる (ヘッダでは void* を使う)。
class TexturePool
{
public:
    struct Key
    {
        QSize size;
        int   format = 0;      ///< QRhiTexture::Format の数値表現
        int   flags = 0;       ///< QRhiTexture::Flags の数値表現

        bool operator==(const Key& o) const
        { return size == o.size && format == o.format && flags == o.flags; }
    };

    /// budgetBytes: プール全体の上限。超過時は LRU 解放する。
    explicit TexturePool(void* rhi, qint64 budgetBytes = 2LL * 1024 * 1024 * 1024);
    ~TexturePool();

    /// 取得。使い終わったら release() で返す。返却されたものは再利用される。
    /// 予算超過かつ解放可能なテクスチャが無い場合は nullptr を返す
    /// (呼び出し側はそのレイヤーの描画をスキップして警告ログを出す)。
    void* acquire(const Key& key);

    void release(void* texture);

    /// 一定フレーム数使われなかったテクスチャを解放する。フレーム末尾で呼ぶ。
    void trim(int unusedFrameThreshold = 120);

    qint64 usedBytes() const { return usedBytes_; }

private:
    struct Entry;

    void* rhi_ = nullptr;
    qint64 budgetBytes_;
    qint64 usedBytes_ = 0;
    int    currentFrame_ = 0;
    std::unordered_multimap<uint64_t, std::unique_ptr<Entry>> pool_;
};

} // namespace yave::render

namespace std {
template <>
struct hash<yave::render::TexturePool::Key>
{
    size_t operator()(const yave::render::TexturePool::Key& k) const noexcept
    {
        size_t h = std::hash<int>()(k.size.width());
        h ^= std::hash<int>()(k.size.height()) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(k.format) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(k.flags) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};
} // namespace std
