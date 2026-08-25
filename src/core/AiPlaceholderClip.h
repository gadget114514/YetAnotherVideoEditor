#pragma once

#include "Clip.h"

#include <QUuid>

namespace yave::ai {
enum class TaskState;   ///< io 層の EnumMapping から文字列変換される (ai/AiGenerationTask.h 参照)
}

namespace yave {

/// AI 生成中 / 未コミットの区間を表すプレースホルダ。
/// 生成完了後に CommitGeneratedAssetCommand で実際のクリップへ置換される。
class AiPlaceholderClip : public Clip
{
public:
    enum class State { Queued, Preparing, Running, PostProcessing, Cached, Failed };

    explicit AiPlaceholderClip(const QUuid& taskId) : taskId_(taskId) {}
    AiPlaceholderClip() = default;

    ClipType type() const override { return ClipType::AiPlaceholder; }

    std::shared_ptr<Clip> clone() const override
    {
        auto c = std::shared_ptr<AiPlaceholderClip>(new AiPlaceholderClip(*this));
        c->setId(QUuid::createUuid());
        return c;
    }

    QUuid taskId() const { return taskId_; }
    void  setTaskId(const QUuid& id) { taskId_ = id; }

    State state() const { return state_; }
    void  setState(State s) { state_ = s; }

    double progress() const { return progress_; }
    void   setProgress(double p) { progress_ = qBound(0.0, p, 1.0); }

    QString modelId() const { return modelId_; }
    void    setModelId(const QString& m) { modelId_ = m; }

    LayerItem makeLayerItem(int64_t frame, int zIndex, const Track& track) const override;

private:
    QUuid   taskId_;
    State   state_    = State::Queued;
    double  progress_ = 0.0;
    QString modelId_;
};

} // namespace yave
