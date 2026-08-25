#include "TransitionCommands.h"

#include "../Project.h"
#include "../Timeline.h"
#include "../Track.h"

#include <QObject>

#include <algorithm>

namespace yave {

namespace {

Track* resolveTrack(Project* project, const QUuid& trackId)
{
    Timeline* tl = project ? project->timeline() : nullptr;
    return tl ? tl->trackById(trackId) : nullptr;
}

std::optional<Transition> transitionById(const Track* track, const QUuid& id)
{
    if (!track)
        return std::nullopt;
    const auto& list = track->transitions();
    const auto it = std::find_if(list.begin(), list.end(),
                                 [&](const Transition& t) { return t.id == id; });
    return it != list.end() ? std::optional<Transition>(*it) : std::nullopt;
}

} // anonymous namespace

// ===========================================================================
//  AddTransitionCommand
// ===========================================================================

AddTransitionCommand::AddTransitionCommand(Project* project, const QUuid& trackId, Transition t)
    : UndoCommandBase(project, QObject::tr("Add transition"))
    , trackId_(trackId)
    , transition_(std::move(t))
{
    if (transition_.id.isNull())
        transition_.id = QUuid::createUuid();
}

void AddTransitionCommand::doRedo()
{
    Track* track = resolveTrack(project(), trackId_);
    if (!track)
        return;

    // 同じ境界にあるものは置き換えられる。Undo で戻せるよう控えておく。
    if (const Transition* existing = track->transitionAtBoundary(transition_.centerFrame))
        replaced_ = *existing;

    error_.clear();
    added_ = track->addTransition(transition_, &error_);
    if (!added_)
        replaced_.reset();
}

void AddTransitionCommand::doUndo()
{
    if (!added_)
        return;
    Track* track = resolveTrack(project(), trackId_);
    if (!track)
        return;

    track->removeTransition(transition_.id);
    if (replaced_)
        track->addTransition(*replaced_);
    added_ = false;
}

// ===========================================================================
//  RemoveTransitionCommand
// ===========================================================================

RemoveTransitionCommand::RemoveTransitionCommand(Project* project, const QUuid& trackId,
                                                 const QUuid& transitionId)
    : UndoCommandBase(project, QObject::tr("Remove transition"))
    , trackId_(trackId)
    , transitionId_(transitionId)
{}

void RemoveTransitionCommand::doRedo()
{
    Track* track = resolveTrack(project(), trackId_);
    if (!track)
        return;
    removed_ = transitionById(track, transitionId_);
    track->removeTransition(transitionId_);
}

void RemoveTransitionCommand::doUndo()
{
    if (!removed_)
        return;
    if (Track* track = resolveTrack(project(), trackId_))
        track->addTransition(*removed_);
}

} // namespace yave
