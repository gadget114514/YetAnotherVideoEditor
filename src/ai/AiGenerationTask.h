#pragma once

#include "AiGenerationParams.h"

#include <QDateTime>
#include <QObject>
#include <QUuid>

#include <atomic>
#include <memory>
#include <vector>

namespace yave {

class Project;
namespace subtitle { class SubtitleClip; }

} // namespace yave

namespace yave::ai {

enum class TaskState
{
    Queued,          ///< 投入済み。実行待ち
    Preparing,       ///< 入力準備中 (参照フレーム抽出など)
    Running,         ///< プロバイダで生成中
    PostProcessing,  ///< 後処理 (fps 合わせ / 尺合わせ)
    Cached,          ///< .yave_cache/gen/<uuid>/ に成果物が存在。コミット待ち
    Committed,       ///< Timeline に反映済み (Undo 可能)
    Failed,
    Cancelled
};

/// 生成成果物 1 個分。
struct GeneratedAsset
{
    enum class Type { Video, Audio, Image, ImageSequence, SubtitleData, Json };

    Type      type = Type::Video;
    /// プロジェクトルートからの相対パス ('/' 区切り)。
    /// 通常は .yave_cache/gen/<task-uuid>/ 配下。collected==true なら assets/generated/ 配下。
    QString   path;
    bool      collected = false;
    QSize     resolution;
    int64_t   durationFrames = 0;
    Rational  frameRate{1001, 60000};
    QJsonObject metadata;     ///< seed, modelVersion 等

    bool operator==(const GeneratedAsset& o) const
    {
        return type == o.type && path == o.path && collected == o.collected
            && resolution == o.resolution && durationFrames == o.durationFrames
            && frameRate == o.frameRate;
    }
};

/// 非同期生成タスク 1 個分の状態コンテナ。
///
/// スレッド安全性:
///   state_ / progress_ / cancelRequested_ のみワーカスレッドから触れる (atomic)。
///   それ以外の setter は UI スレッドからのみ呼ぶこと。
class AiGenerationTask : public QObject
{
    Q_OBJECT
public:
    explicit AiGenerationTask(AiGenerationParams params, QObject* parent = nullptr);

    QUuid                     id() const { return id_; }
    TaskState                 state() const { return state_.load(std::memory_order_relaxed); }
    const AiGenerationParams& params() const { return params_; }
    AiGenerationParams&       mutableParams() { return params_; }
    double                    progress() const { return progress_.load(std::memory_order_relaxed); }
    QString                   errorMessage() const { return error_; }
    int                       retryCount() const { return retryCount_; }
    QDateTime                 createdAt() const { return createdAt_; }
    void                      setCreatedAt(const QDateTime& t) { createdAt_ = t; }
    QDateTime                 completedAt() const { return completedAt_; }
    void                      setCompletedAt(const QDateTime& t) { completedAt_ = t; }

    const std::vector<GeneratedAsset>& assets() const { return assets_; }
    std::vector<GeneratedAsset>&       mutableAssets() { return assets_; }

    /// 状態を変更する。UI スレッドから呼ぶこと (シグナル発火を伴う)。
    void setState(TaskState s);
    void setProgress(double p);
    void setError(const QString& message);
    void incrementRetryCount() { ++retryCount_; }

    /// ワーカスレッドから呼んでよい (atomic フラグを立てるだけ)。
    void requestCancel() { cancelRequested_.store(true, std::memory_order_relaxed); }
    bool isCancelRequested() const
    { return cancelRequested_.load(std::memory_order_relaxed); }

    /// ディスクへの永続化 / 復帰用 (aiTasks JSON 要素)
    QJsonObject toJson() const;

signals:
    void stateChanged(TaskState s);
    void progressChanged(double p);
    void failed(const QString& message);

private:
    QUuid                       id_;
    AiGenerationParams          params_;
    std::atomic<TaskState>      state_{TaskState::Queued};
    std::atomic<double>         progress_{0.0};
    std::atomic<bool>           cancelRequested_{false};
    std::vector<GeneratedAsset> assets_;
    QString                     error_;
    int                         retryCount_ = 0;
    QDateTime                   createdAt_;
    QDateTime                   completedAt_;
};

} // namespace yave::ai
