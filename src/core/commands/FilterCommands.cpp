#include "FilterCommands.h"

#include "../Clip.h"
#include "../Project.h"
#include "../Timeline.h"

#include <QObject>

namespace yave {

namespace {

std::shared_ptr<Clip> findClip(Project* project, const QUuid& clipId)
{
    Timeline* tl = project ? project->timeline() : nullptr;
    return tl ? tl->findClip(clipId) : nullptr;
}

} // anonymous namespace

// ===========================================================================
//  AddFilterCommand
// ===========================================================================

AddFilterCommand::AddFilterCommand(Project* project, const QUuid& clipId,
                                   VideoFilterInstance inst, int index)
    : UndoCommandBase(project, QObject::tr("Add filter"))
    , clipId_(clipId)
    , inst_(std::move(inst))
    , index_(index)
{}

void AddFilterCommand::doRedo()
{
    auto clip = findClip(project(), clipId_);
    if (!clip)
        return;
    if (index_ < 0)
        index_ = int(clip->filters().size());
    clip->insertFilter(index_, inst_);
    added_ = true;
}

void AddFilterCommand::doUndo()
{
    if (!added_)
        return;
    if (auto clip = findClip(project(), clipId_))
        clip->removeFilter(index_);
    added_ = false;
}

// ===========================================================================
//  RemoveFilterCommand
// ===========================================================================

RemoveFilterCommand::RemoveFilterCommand(Project* project, const QUuid& clipId, int index)
    : UndoCommandBase(project, QObject::tr("Remove filter"))
    , clipId_(clipId)
    , index_(index)
{
    if (auto clip = findClip(project, clipId)) {
        if (index >= 0 && index < int(clip->filters().size())) {
            removed_ = clip->filters()[size_t(index)];
            valid_   = true;
        }
    }
}

void RemoveFilterCommand::doRedo()
{
    if (!valid_)
        return;
    if (auto clip = findClip(project(), clipId_))
        clip->removeFilter(index_);
}

void RemoveFilterCommand::doUndo()
{
    if (!valid_)
        return;
    if (auto clip = findClip(project(), clipId_))
        clip->insertFilter(index_, removed_);
}

// ===========================================================================
//  ReorderFilterCommand
// ===========================================================================

ReorderFilterCommand::ReorderFilterCommand(Project* project, const QUuid& clipId,
                                           int from, int to)
    : UndoCommandBase(project, QObject::tr("Reorder filters"))
    , clipId_(clipId)
    , from_(from)
    , to_(to)
{}

void ReorderFilterCommand::doRedo()
{
    if (auto clip = findClip(project(), clipId_))
        clip->moveFilter(from_, to_);
}

void ReorderFilterCommand::doUndo()
{
    if (auto clip = findClip(project(), clipId_))
        clip->moveFilter(to_, from_);
}

} // namespace yave
