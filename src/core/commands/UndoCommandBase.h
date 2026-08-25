#pragma once

#include <QUndoCommand>
#include <QString>

namespace yave {

class Project;

/// すべての編集コマンドの基底。
/// 目的:
///   - コマンド ID による自動マージ (連続したドラッグを 1 コマンドにまとめる)
///   - 実行後の副作用通知 (音声グラフ再構築の要求など) を一元化
class UndoCommandBase : public QUndoCommand
{
public:
    enum CommandId
    {
        IdNone              = -1,
        IdMoveClip          = 1,
        IdTrimClip          = 2,
        IdChangeOpacity     = 3,
        IdChangeGain        = 4,
        IdEditSubtitleText  = 5,
        IdChangeEffectParam = 6,
    };

    explicit UndoCommandBase(Project* project, const QString& text)
        : QUndoCommand(text), project_(project) {}

    int  id() const override { return IdNone; }
    bool mergeWith(const QUndoCommand* other) override { Q_UNUSED(other); return false; }

    void redo() override final;
    void undo() override final;

protected:
    /// 派生クラスが実装する実処理
    virtual void doRedo() = 0;
    virtual void doUndo() = 0;

    /// この操作が音声グラフの再構築を要求するか
    virtual bool affectsAudioGraph() const { return false; }

    /// この操作がレンダリングスナップショットを無効化するか
    virtual bool affectsRendering() const { return true; }

    Project* project() const { return project_; }

private:
    void notifyAfterChange();

    Project* project_ = nullptr;
};

} // namespace yave
