#pragma once

#include "AiGenerationParams.h"
#include "AiGenerationTask.h"
#include "GenerationCache.h"
#include "ProviderRegistry.h"

#include <QMutex>
#include <QObject>
#include <QSet>
#include <QThreadPool>
#include <QUuid>

#include <memory>
#include <vector>

namespace yave {

class Project;
class Clip;

} // namespace yave

namespace yave::render {
/// 前方宣言のみ。yave_ai は yave_render へのコンパイル時依存を持たない。
/// 参照フレーム書き出しは setReferenceFrameSink() 経由で注入する。
class RhiCompositor;
}

namespace yave::ai {

/// I2V / V2V の参照フレームをファイルへ書き出す処理の注入ポイント。
/// (trackId, frame, outputFilePath)。yave_render 側の実装を app 層が接続する。
using ReferenceFrameSink = std::function<void(const QUuid& trackId, int64_t frame,
                                              const QString& outputPath)>;
void setReferenceFrameSink(ReferenceFrameSink fn);
bool hasReferenceFrameSink();

/// 非同期で各生成タスクを管理・実行するコアクラス。
///
/// 責務:
///   - タスクの投入とキューイング
///   - プレースホルダクリップの即時配置
///   - プロバイダの選択と実行
///   - 生成物のキャッシュと Timeline へのコミット (Undo 可能)
///   - セッション復帰
///
/// スレッド:
///   public メソッドは UI スレッドから呼ぶ。
///   実行は QThreadPool 上で行われ、完了通知は queued connection で UI へ戻る。
class AiGenerationOrchestrator : public QObject
{
    Q_OBJECT
public:
    /// compositor は I2V の参照フレーム書き出しに使う。無くても動作する (nullptr 可)。
    explicit AiGenerationOrchestrator(Project* project,
                                      render::RhiCompositor* compositor = nullptr,
                                      QObject* parent = nullptr);
    ~AiGenerationOrchestrator() override;

    // ================= 投入 =================

    /// タスクを投入する。検証失敗なら null QUuid を返し taskFailed を発火する。
    /// 成功時は即座にプレースホルダクリップが配置される (Undo 可能)。
    QUuid submit(const AiGenerationParams& params);

    /// 同一パラメータでの再生成。newSeed=true なら seed を振り直す。
    QUuid regenerate(const QUuid& taskId, bool newSeed);
    void retry(const QUuid& taskId);

    // ================= 制御 =================

    void cancel(const QUuid& taskId);
    void cancelAll();
    bool waitForDone(int timeoutMs);

    // ================= コミット =================

    /// Cached 状態のタスクを Timeline へ反映する (Undo 可能)。
    void commit(const QUuid& taskId);
    /// 生成物を破棄し、プレースホルダを削除する。
    void discard(const QUuid& taskId);

    void setAutoCommit(bool on) { autoCommit_ = on; }
    bool autoCommit() const { return autoCommit_; }

    // ================= 照会 =================

    std::vector<AiGenerationTask*> tasks() const;
    AiGenerationTask*              task(const QUuid& id) const;
    int                            activeTaskCount() const;

    ProviderRegistry* providerRegistry() const { return registry_; }
    GenerationCache*  cache() const { return cache_.get(); }

    // ================= 永続化 =================

    void persistToDisk();
    void restoreFromDisk();

signals:
    void taskAdded(const QUuid& id);
    void taskStateChanged(const QUuid& id, TaskState state);
    void taskProgressChanged(const QUuid& id, double progress);
    void taskCached(const QUuid& id);
    void taskCommitted(const QUuid& id);
    void taskFailed(const QUuid& id, const QString& messageKey);
    void activeTaskCountChanged(int count);

private:
    QString cacheRoot() const;
    QString workDirFor(const AiGenerationTask* task) const;

    /// ワーカスレッドへ投入する
    void runTask(AiGenerationTask* task);
    GenerationOutput executeWithProvider(AiGenerationTask* task,
                                         const AiGenerationParams& params);

    /// I2V の参照画像を解決してファイルパスを返す
    QString resolveImageRef(const ImageReference& ref, int64_t defaultFrame,
                            const QDir& workDir, const QString& fileName);
    std::vector<GeneratedAsset> postProcess(const AiGenerationParams& p,
                                            const GenerationOutput& out);
    void onTaskCached(AiGenerationTask* task);
    std::shared_ptr<Clip> makeClipFromAsset(const AiGenerationTask& t,
                                            const GeneratedAsset& a);

    Project*                               project_    = nullptr;
    render::RhiCompositor*                 compositor_ = nullptr;
    ProviderRegistry*                      registry_   = nullptr;
    std::unique_ptr<GenerationCache>       cache_;

    QThreadPool                            pool_;
    mutable QMutex                         mutex_;
    std::vector<std::unique_ptr<AiGenerationTask>> tasks_;
    QHash<QUuid, std::shared_ptr<Clip>>    placeholderByTask_;
    QSet<QUuid>                            cancelledTasks_;

    bool                                   autoCommit_ = false;
    std::atomic<int>                       activeCount_{0};
};

} // namespace yave::ai
