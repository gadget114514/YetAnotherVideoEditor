#pragma once

#include "../Transition.h"
#include "UndoCommandBase.h"

#include <QUuid>

#include <optional>

namespace yave {

/// クリップ境界へトランジションを置く (3.10)。
/// ハンドルが足りない場合、Track 側が収まる長さへ縮める。
/// 同じ境界に既にある場合は置き換え、Undo で元のものへ戻す。
class AddTransitionCommand : public UndoCommandBase
{
public:
    AddTransitionCommand(Project* project, const QUuid& trackId, Transition t);

    /// 置けなかった理由 (置けた場合は空)。UI がコンソールへ出す。
    QString error() const { return error_; }

protected:
    void doRedo() override;
    void doUndo() override;

private:
    QUuid                     trackId_;
    Transition                transition_;
    std::optional<Transition> replaced_;   ///< 置き換えた既存のもの
    bool                      added_ = false;
    QString                   error_;
};

/// トランジションを外す。
class RemoveTransitionCommand : public UndoCommandBase
{
public:
    RemoveTransitionCommand(Project* project, const QUuid& trackId, const QUuid& transitionId);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    QUuid                     trackId_;
    QUuid                     transitionId_;
    std::optional<Transition> removed_;
};

} // namespace yave
