#pragma once

#include "../VideoFilter.h"
#include "UndoCommandBase.h"

#include <QUuid>

namespace yave {

/// クリップのフィルタースタックへ 1 段追加する (3.9)。
/// index < 0 なら末尾へ積む。
class AddFilterCommand : public UndoCommandBase
{
public:
    AddFilterCommand(Project* project, const QUuid& clipId,
                     VideoFilterInstance inst, int index = -1);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    QUuid               clipId_;
    VideoFilterInstance inst_;
    int                 index_ = -1;
    bool                added_ = false;
};

/// フィルターを 1 段外す。
class RemoveFilterCommand : public UndoCommandBase
{
public:
    RemoveFilterCommand(Project* project, const QUuid& clipId, int index);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    QUuid               clipId_;
    int                 index_ = -1;
    VideoFilterInstance removed_;
    bool                valid_ = false;
};

/// フィルターの適用順を入れ替える。
class ReorderFilterCommand : public UndoCommandBase
{
public:
    ReorderFilterCommand(Project* project, const QUuid& clipId, int from, int to);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    QUuid clipId_;
    int   from_ = 0;
    int   to_   = 0;
};

} // namespace yave
