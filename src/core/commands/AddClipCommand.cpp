#include "AddClipCommand.h"
#include "../Clip.h"
#include "../Project.h"
#include "../Timeline.h"
#include "../Track.h"

#include <QtGlobal>

namespace yave {

namespace {

Track* resolveTrack(Project* project, const QUuid& trackId, int trackIndex)
{
    Timeline* tl = project->timeline();
    if (!tl)
        return nullptr;
    if (!trackId.isNull())
        return tl->trackById(trackId);
    return tl->trackAt(trackIndex);
}

} // anonymous namespace

// ===========================================================================
//  AddClipCommand
// ===========================================================================

AddClipCommand::AddClipCommand(Project* project, const QUuid& trackId, int trackIndex,
                               const std::shared_ptr<Clip>& clip)
    : UndoCommandBase(project, QObject::tr("Add clip"))
    , trackId_(trackId)
    , trackIndex_(trackIndex)
    , clip_(clip)
{}

void AddClipCommand::doRedo()
{
    Track* t = resolveTrack(project(), trackId_, trackIndex_);
    if (!t || !clip_)
        return;
    if (t->insertClip(clip_))
        inserted_ = true;
}

void AddClipCommand::doUndo()
{
    if (!inserted_)
        return;
    Track* t = resolveTrack(project(), trackId_, trackIndex_);
    if (t)
        t->removeClip(clip_->id());
    inserted_ = false;
}

// ===========================================================================
//  RemoveClipCommand
// ===========================================================================

RemoveClipCommand::RemoveClipCommand(Project* project, const QUuid& trackId,
                                     const QUuid& clipId)
    : UndoCommandBase(project, QObject::tr("Delete clip"))
    , trackId_(trackId)
    , clipId_(clipId)
{}

void RemoveClipCommand::doRedo()
{
    Track* t = project()->timeline()->trackById(trackId_);
    if (!t)
        return;
    removed_ = t->takeClip(clipId_);
}

void RemoveClipCommand::doUndo()
{
    if (!removed_)
        return;
    Track* t = project()->timeline()->trackById(trackId_);
    if (t && t->insertClip(removed_))
        removed_.reset();
}

// ===========================================================================
//  MoveClipCommand
// ===========================================================================

MoveClipCommand::MoveClipCommand(Project* project,
                                 const QUuid& fromTrackId, const QUuid& toTrackId,
                                 const QUuid& clipId, const TimeRange& newRange)
    : UndoCommandBase(project, QObject::tr("Move clip"))
    , clipId_(clipId)
{
    before_.trackId = fromTrackId;

    // 現在の range を取得する (コマンド生成時点の値が「移動前」)
    Track* src = project->timeline()->trackById(fromTrackId);
    if (src) {
        if (auto c = src->clipById(clipId))
            before_.range = c->range();
    }

    after_.trackId = toTrackId;
    after_.range   = newRange;
}

void MoveClipCommand::doRedo()
{
    Timeline* tl = project()->timeline();

    std::shared_ptr<Clip> clip = tl->findClip(clipId_);
    if (!clip) {
        // 初回 redo: クリップはまだ古い位置にいる
        Track* src = tl->trackById(before_.trackId);
        if (!src)
            return;
        clip = src->takeClip(clipId_);
        if (!clip)
            return;
        clip->setRange(after_.range);

        Track* dst = tl->trackById(after_.trackId);
        if (!dst || !dst->acceptsClip(*clip)) {
            src->insertClip(clip);   // 戻す
            return;
        }
        dst->insertClip(clip);
        return;
    }

    // 2 回目以降の redo (undo からの復帰): undo で戻した状態から after へ
    // findClip は全トラックを見るので、undo 後は before 側にいる。
    for (int i = 0; i < tl->trackCount(); ++i) {
        Track* t = tl->trackAt(i);
        if (t->clipById(clipId_))
            t->takeClip(clipId_);
    }
    clip->setRange(after_.range);
    Track* dst = tl->trackById(after_.trackId);
    if (dst)
        dst->insertClip(clip);
}

void MoveClipCommand::doUndo()
{
    Timeline* tl = project()->timeline();
    auto clip = tl->findClip(clipId_);
    if (!clip)
        return;

    for (int i = 0; i < tl->trackCount(); ++i) {
        Track* t = tl->trackAt(i);
        if (t->clipById(clipId_))
            t->takeClip(clipId_);
    }
    clip->setRange(before_.range);
    Track* dst = tl->trackById(before_.trackId);
    if (dst)
        dst->insertClip(clip);
}

bool MoveClipCommand::mergeWith(const QUndoCommand* other)
{
    const auto* o = dynamic_cast<const MoveClipCommand*>(other);
    if (!o || o->clipId_ != clipId_)
        return false;
    // 連続ドラッグ: 直前のコマンドの after を引き継ぐ (before は不変)
    after_   = o->after_;
    firstRun_ = false;
    setText(QObject::tr("Move clip"));
    return true;
}

// ===========================================================================
//  TrimClipCommand
// ===========================================================================

TrimClipCommand::TrimClipCommand(Project* project, const QUuid& trackId,
                                 const QUuid& clipId, int64_t newSourceOffset,
                                 const TimeRange& newRange)
    : UndoCommandBase(project, QObject::tr("Trim clip"))
    , trackId_(trackId)
    , clipId_(clipId)
    , newOffset_(newSourceOffset)
    , newRange_(newRange)
{
    Track* t = project->timeline()->trackById(trackId_);
    if (t) {
        if (auto c = t->clipById(clipId_)) {
            oldOffset_ = c->sourceOffset();
            oldRange_  = c->range();
        }
    }
}

void TrimClipCommand::doRedo()
{
    Track* t = project()->timeline()->trackById(trackId_);
    if (!t)
        return;
    auto c = t->clipById(clipId_);
    if (!c)
        return;
    if (firstRun_) {
        firstRun_ = false;
    }
    c->setSourceOffset(newOffset_);
    c->setRange(newRange_);
}

void TrimClipCommand::doUndo()
{
    Track* t = project()->timeline()->trackById(trackId_);
    if (!t)
        return;
    auto c = t->clipById(clipId_);
    if (!c)
        return;
    c->setSourceOffset(oldOffset_);
    c->setRange(oldRange_);
}

bool TrimClipCommand::mergeWith(const QUndoCommand* other)
{
    const auto* o = dynamic_cast<const TrimClipCommand*>(other);
    if (!o || o->clipId_ != clipId_)
        return false;
    newOffset_ = o->newOffset_;
    newRange_  = o->newRange_;
    setText(QObject::tr("Trim clip"));
    return true;
}

} // namespace yave
