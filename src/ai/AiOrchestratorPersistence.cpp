#include "AiGenerationOrchestrator.h"
#include "AiGenerationTask.h"

#include "../core/Project.h"
#include "../core/Timeline.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace yave::ai {

// ===========================================================================
//  セッション復帰 (.yave_cache/tasks.json)
//
//  Running だったタスクは Queued に戻して再投入する。生成パラメータが
//  完全に保存されているため、乱数シード固定の生成であれば同一結果が得られる。
// ===========================================================================

void AiGenerationOrchestrator::persistToDisk()
{
    if (!project_)
        return;

    QJsonArray arr;
    for (AiGenerationTask* t : tasks()) {
        if (!t || t->state() == TaskState::Cancelled)
            continue;
        arr.append(t->toJson());
    }

    QSaveFile f(cacheRoot() + QStringLiteral("/tasks.json"));
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    f.commit();
}

void AiGenerationOrchestrator::restoreFromDisk()
{
    QFile f(cacheRoot() + QStringLiteral("/tasks.json"));
    if (!f.open(QIODevice::ReadOnly))
        return;

    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        auto params = AiGenerationParams::fromJson(o[QStringLiteral("params")].toObject());

        auto t = std::make_unique<AiGenerationTask>(params);
        // id の復元 (保存されたタスクとして同一視できるように)
        const QUuid restoredId(o[QStringLiteral("id")].toString());
        if (!restoredId.isNull()) {
            // AiGenerationTask は生成時に id を振るため、復元時は新規 id を許容する。
            // (プロジェクト JSON 側の aiTasks 配列との突き合わせは io 層が行う)
            Q_UNUSED(restoredId);
        }
        const int savedState = o[QStringLiteral("state")].toInt(int(TaskState::Queued));

        const QUuid newId = t->id();
        connect(t.get(), &AiGenerationTask::progressChanged, this,
                [this, newId](double p) {
                    emit taskProgressChanged(newId, p);
                    if (project_)
                        project_->timeline()->updatePlaceholderProgress(newId, p);
                });

        AiGenerationTask* raw = t.get();
        {
            QMutexLocker lock(&mutex_);
            tasks_.push_back(std::move(t));
        }
        emit taskAdded(newId);

        // Running / Cached だったものは Queued に戻して再投入する。
        if (savedState == int(TaskState::Running) || savedState == int(TaskState::Queued))
            runTask(raw);
        else if (savedState == int(TaskState::Failed))
            raw->setState(TaskState::Queued);
    }
}

} // namespace yave::ai
