#include "AiController.h"

#include "../../ai/AiGenerationOrchestrator.h"
#include "../../core/Project.h"

namespace yave {

AiController::AiController(QObject* parent) : QObject(parent) {}

void AiController::attachProject(Project* project)
{
    project_ = project;
}

ai::AiGenerationParams AiController::paramsFromRequest(const QVariantMap& m) const
{
    ai::AiGenerationParams p;

    p.kind = ai::GenerationKind(m.value(QStringLiteral("kind"), 0).toInt());
    p.range.start    = m.value(QStringLiteral("startFrame")).toLongLong();
    p.range.duration = m.value(QStringLiteral("durationFrames")).toLongLong();
    p.prompt         = m.value(QStringLiteral("prompt")).toString();
    p.negativePrompt = m.value(QStringLiteral("negativePrompt")).toString();
    p.modelId        = m.value(QStringLiteral("modelId")).toString();
    p.providerId     = m.value(QStringLiteral("providerId")).toString();
    p.seed           = m.value(QStringLiteral("seed"), qint64(-1)).toLongLong();
    p.steps          = m.value(QStringLiteral("steps"), 30).toInt();
    p.guidanceScale  = m.value(QStringLiteral("guidanceScale"), 7.5).toDouble();

    if (p.kind == ai::GenerationKind::Subtitle) {
        p.subtitleMode = ai::SubtitleGenMode(
            m.value(QStringLiteral("subtitleMode"), 0).toInt());
        p.language = m.value(QStringLiteral("language"), QStringLiteral("ja")).toString();
    }
    if (p.kind == ai::GenerationKind::Audio) {
        p.audioMode = ai::AudioGenMode(m.value(QStringLiteral("audioMode"), 0).toInt());
        p.voiceId   = m.value(QStringLiteral("voiceId")).toString();
    }

    return p;
}

QVariantMap AiController::submitTask(const QVariantMap& request)
{
    QVariantMap out;
    if (!project_) {
        out[QStringLiteral("ok")] = false;
        out[QStringLiteral("errorKey")] = QStringLiteral("error.ai.noProject");
        return out;
    }

    // Orchestrator は app 層で生成済み。ProjectController 経由で取得する設計だが、
    // 簡略化のためここでは遅延生成する。
    static ai::AiGenerationOrchestrator orchestrator(project_);
    orchestrator.setAutoCommit(project_->isAutoCommitAi());

    const auto params = paramsFromRequest(request);
    const QUuid id = orchestrator.submit(params);

    out[QStringLiteral("ok")]      = !id.isNull();
    out[QStringLiteral("taskId")]  = id.toString(QUuid::WithoutBraces);
    if (id.isNull())
        out[QStringLiteral("errorKey")] = QStringLiteral("error.ai.validation");
    return out;
}

void AiController::cancel(const QUuid& taskId)
{
    static_cast<void>(taskId);
}

void AiController::commit(const QUuid& taskId)
{
    static_cast<void>(taskId);
}

void AiController::discard(const QUuid& taskId)
{
    static_cast<void>(taskId);
}

} // namespace yave
