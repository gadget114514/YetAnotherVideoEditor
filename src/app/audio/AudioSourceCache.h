#pragma once

#include "../../audio/AudioRenderGraph.h"

#include <QHash>
#include <QObject>
#include <QUuid>

#include <memory>

namespace yave {

class Project;
class Asset;

namespace app {

/// 素材 (AssetLibrary) の音声を float PCM へデコードしてグラフへ供給するキャッシュ。
///
/// UI スレッドのグラフ構築 (AudioRenderGraphBuilder) から呼ばれる。
/// デコード結果は assetId ごとに保持し、同一素材の再デコードを避ける。
class AudioSourceCache : public QObject, public audio::IAudioSourceProvider
{
    Q_OBJECT
public:
    explicit AudioSourceCache(QObject* parent = nullptr);

    void setProject(Project* project);

    /// IAudioSourceProvider: assetId の音声をデコードして返す (キャッシュあり)。
    std::shared_ptr<audio::DecodedAudio> decodeAsset(const QUuid& assetId) override;

    /// キャッシュを空にする (プロジェクト切替時)。
    void clear();

private:
    Project* project_ = nullptr;
    QHash<QUuid, std::shared_ptr<audio::DecodedAudio>> cache_;
};

} // namespace yave::app
} // namespace yave