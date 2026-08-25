#include "AiGenerationTask.h"

#include <QJsonArray>

namespace yave::ai {

AiGenerationTask::AiGenerationTask(AiGenerationParams params, QObject* parent)
    : QObject(parent)
    , id_(QUuid::createUuid())
    , params_(std::move(params))
    , createdAt_(QDateTime::currentDateTimeUtc())
{}

void AiGenerationTask::setState(TaskState s)
{
    if (state_.exchange(s, std::memory_order_relaxed) == s)
        return;
    if (s == TaskState::Cached || s == TaskState::Committed
        || s == TaskState::Failed || s == TaskState::Cancelled)
        completedAt_ = QDateTime::currentDateTimeUtc();
    emit stateChanged(s);
}

void AiGenerationTask::setProgress(double p)
{
    const double clamped = qBound(0.0, p, 1.0);
    progress_.store(clamped, std::memory_order_relaxed);
    emit progressChanged(clamped);
}

void AiGenerationTask::setError(const QString& message)
{
    error_ = message;
    setState(TaskState::Failed);
    emit failed(message);
}

// ===========================================================================
//  永続化 (.yave_cache/tasks.json 用の最小形。プロジェクト JSON への書き出しは io 層)
// ===========================================================================

QJsonObject AiGenerationTask::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("id")]        = id_.toString(QUuid::WithoutBraces);
    o[QStringLiteral("state")]     = int(state_.load(std::memory_order_relaxed));
    o[QStringLiteral("progress")]  = progress_.load(std::memory_order_relaxed);
    o[QStringLiteral("createdAt")] = createdAt_.toString(Qt::ISODate);
    if (completedAt_.isValid())
        o[QStringLiteral("completedAt")] = completedAt_.toString(Qt::ISODate);
    o[QStringLiteral("retryCount")] = retryCount_;
    if (!error_.isEmpty())
        o[QStringLiteral("error")] = error_;
    o[QStringLiteral("params")] = params_.toJson();

    QJsonArray assets;
    for (const GeneratedAsset& a : assets_) {
        QJsonObject ao;
        ao[QStringLiteral("type")]           = int(a.type);
        ao[QStringLiteral("path")]           = a.path;
        ao[QStringLiteral("collected")]      = a.collected;
        ao[QStringLiteral("width")]          = a.resolution.width();
        ao[QStringLiteral("height")]         = a.resolution.height();
        ao[QStringLiteral("durationFrames")] = double(a.durationFrames);
        ao[QStringLiteral("fpsNum")]         = double(a.frameRate.num);
        ao[QStringLiteral("fpsDen")]         = double(a.frameRate.den);
        ao[QStringLiteral("metadata")]       = a.metadata;
        assets.append(ao);
    }
    o[QStringLiteral("assets")] = assets;
    return o;
}

} // namespace yave::ai
