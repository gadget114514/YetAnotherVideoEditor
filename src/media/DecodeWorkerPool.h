#pragma once

#include "FrameCache.h"
#include "VideoDecoder.h"

#include <QThreadPool>
#include <QUuid>

#include <memory>
#include <vector>

namespace yave::media {

/// デコード先行読み込み (look-ahead) ワーカプール。
///
/// 推奨スレッド数 = min(物理コア数, 同時可視ビデオトラック数 + 2)。
/// 結果は FrameQueue 経由で Render Thread へ渡される (1.3 スレッドモデル)。
class DecodeWorkerPool
{
public:
    explicit DecodeWorkerPool(int maxThreads = 4)
    {
        pool_.setMaxThreadCount(maxThreads > 0 ? maxThreads : 1);
    }

    /// 先読み要求。指定フレーム群がキャッシュに無い場合のみデコードする。
    void requestLookAhead(std::shared_ptr<VideoDecoder> decoder,
                          std::shared_ptr<FrameCache> cache,
                          const QUuid& assetId,
                          std::vector<int64_t> sourceFrames);

    void shutdown() { pool_.clear(); pool_.waitForDone(3000); }

private:
    QThreadPool pool_;
};

} // namespace yave::media
