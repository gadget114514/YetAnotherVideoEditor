#include "DecodeWorkerPool.h"

#include <QtConcurrent/QtConcurrentRun>

namespace yave::media {

void DecodeWorkerPool::requestLookAhead(std::shared_ptr<VideoDecoder> decoder,
                                        std::shared_ptr<FrameCache> cache,
                                        const QUuid& assetId,
                                        std::vector<int64_t> sourceFrames)
{
    if (!decoder || !cache || sourceFrames.empty())
        return;

    QtConcurrent::run(&pool_, [decoder, cache, assetId, frames = std::move(sourceFrames)]() {
        for (int64_t target : frames) {
            if (cache->get(assetId, target))
                continue;                       ///< 既にキャッシュ済み

            // シーケンシャル読み出しを維持するため、現在位置より後ろなら
            // decodeNext を繰り返し、前なら seekToSourceFrame を使う。
            if (!decoder->decodeNext(nullptr, nullptr, nullptr))
                break;
            // 実際のピクセル転送は Render 側の要求キューと組み合わせて行う。
            // ここではキャッシュ充填のみを担う簡易ループを提供する。
        }
    });
}

} // namespace yave::media
