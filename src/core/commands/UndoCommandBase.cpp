#include "UndoCommandBase.h"
#include "../Project.h"
#include "../Timeline.h"

namespace yave {

void UndoCommandBase::redo()
{
    doRedo();
    notifyAfterChange();
}

void UndoCommandBase::undo()
{
    doUndo();
    notifyAfterChange();
}

void UndoCommandBase::notifyAfterChange()
{
    if (!project_)
        return;
    project_->markModified();
    if (affectsRendering() && project_->timeline()) {
        // RenderSnapshot は毎フレーム再生成されるので、ここでは revision を進めるだけ。
        emit project_->timeline()->structureChanged();
    }
}

} // namespace yave
