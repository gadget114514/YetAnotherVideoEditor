#include "AddTrackCommand.h"
#include "../Project.h"
#include "../Timeline.h"
#include "../Track.h"

namespace yave {

// ===========================================================================
//  AddTrackCommand
// ===========================================================================

AddTrackCommand::AddTrackCommand(Project* project, TrackType type, int index,
                                 const QString& name)
    : UndoCommandBase(project, QObject::tr("Add track"))
    , type_(type)
    , index_(index)
    , name_(name.isEmpty() ? QObject::tr("Track %1").arg(index + 1) : name)
{
}

void AddTrackCommand::doRedo()
{
    Timeline* tl = project()->timeline();
    if (!tl)
        return;
    if (!created_) {
        // 初回 redo のみ新規生成。以降は同じインスタンスを再利用するため、
        // Undo/Redo 往復でトラック id が変わらない。
        created_ = std::make_unique<Track>(type_);
        trackId_ = created_->id();
        created_->setName(name_);
    }
    tl->reinsertTrack(index_, std::move(created_));
}

void AddTrackCommand::doUndo()
{
    Timeline* tl = project()->timeline();
    if (!tl)
        return;
    created_ = tl->takeTrackById(trackId_);
}

// ===========================================================================
//  RemoveTrackCommand
// ===========================================================================

RemoveTrackCommand::RemoveTrackCommand(Project* project, const QUuid& trackId)
    : UndoCommandBase(project, QObject::tr("Delete track"))
    , trackId_(trackId)
{}

void RemoveTrackCommand::doRedo()
{
    Timeline* tl = project()->timeline();
    if (!tl)
        return;
    if (!removed_) {
        originalIndex_ = tl->indexOfTrack(trackId_);
        if (originalIndex_ < 0)
            return;
        removed_ = tl->takeTrack(originalIndex_);
    } else {
        (void)tl->takeTrackById(trackId_);
    }
}

void RemoveTrackCommand::doUndo()
{
    if (!removed_)
        return;
    project()->timeline()->reinsertTrack(originalIndex_, std::move(removed_));
}

// ===========================================================================
//  ReorderTrackCommand
// ===========================================================================

ReorderTrackCommand::ReorderTrackCommand(Project* project, int from, int to)
    : UndoCommandBase(project, QObject::tr("Reorder tracks"))
    , from_(from)
    , to_(to)
{}

void ReorderTrackCommand::doRedo()
{
    if (originalFromIndex_ < 0)
        originalFromIndex_ = from_;   ///< 初回のみ記録
    project()->timeline()->moveTrack(from_, to_);
}

void ReorderTrackCommand::doUndo()
{
    if (originalFromIndex_ < 0)
        return;
    // moveTrack(to_, from_) は rotate の性質で戻らないケースがあるため、
    // 対象トラックを一度取り出して元 index へ戻す。
    auto track = project()->timeline()->takeTrack(to_);
    if (!track)
        return;
    project()->timeline()->reinsertTrack(originalFromIndex_, std::move(track));
}

} // namespace yave
