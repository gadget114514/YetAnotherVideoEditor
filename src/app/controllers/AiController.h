#pragma once

#include "../ai/AiGenerationParams.h"
#include "../ai/AiGenerationOrchestrator.h"
#include "../core/Rational.h"

#include <QObject>
#include <QUuid>

namespace yave {

class Project;

/// AI 生成の UI ブリッジ。
/// AiGenerationOrchestrator のシグナルを QML へ中継する。
class AiController : public QObject
{
    Q_OBJECT
public:
    explicit AiController(QObject* parent = nullptr);

    void attachProject(Project* project);

    Q_INVOKABLE QVariantMap submitTask(const QVariantMap& request);
    Q_INVOKABLE void cancel(const QUuid& taskId);
    Q_INVOKABLE void commit(const QUuid& taskId);
    Q_INVOKABLE void discard(const QUuid& taskId);

signals:
    void taskAdded(const QUuid& id);
    void taskProgressChanged(const QUuid& id, double progress);
    void taskCached(const QUuid& id);
    void taskCommitted(const QUuid& id);
    void taskFailed(const QUuid& id, const QString& messageKey);

private:
    ai::AiGenerationParams paramsFromRequest(const QVariantMap& map) const;

    Project* project_ = nullptr;
};

} // namespace yave
