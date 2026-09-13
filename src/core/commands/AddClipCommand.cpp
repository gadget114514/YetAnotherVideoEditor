#include "AddClipCommand.h"
#include "../Clip.h"
#include "../Project.h"
#include "../Timeline.h"
#include "../Track.h"

#include <QObject>
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
    rejectReason_.clear();

    Track* t = resolveTrack(project(), trackId_, trackIndex_);
    if (!t || !clip_) {
        rejectReason_ = QObject::tr("The drop target track no longer exists.");
        qInfo() << "[AddClipCommand] redo: no track/clip (trackId:"
                << trackId_ << "trackIndex:" << trackIndex_ << ")";
        return;
    }
    // 型互換チェック: 音声クリップを映像トラックへ置く等を防ぐ。
    if (!t->acceptsClip(*clip_)) {
        rejectReason_ = QObject::tr("This clip type cannot be placed on that track.");
        qInfo() << "[AddClipCommand] redo: REJECTED type mismatch (clipType:"
                << int(clip_->type()) << "trackType:" << int(t->type())
                << "trackId:" << t->id() << ")";
        return;
    }
    if (t->insertClip(clip_)) {
        inserted_ = true;
        qInfo() << "[AddClipCommand] redo: INSERTED clip" << clip_->id()
                << "trackId:" << t->id()
                << "range:" << clip_->range().start << "->" << clip_->range().end();
    } else {
        rejectReason_ = QObject::tr("This position overlaps an existing clip.");
        qInfo() << "[AddClipCommand] redo: INSERT FAILED (overlap?) trackId:" << t->id()
                << "range:" << clip_->range().start << "->" << clip_->range().end();
    }
}

void AddClipCommand::doUndo()
{
    if (!inserted_)
        return;
    Track* t = resolveTrack(project(), trackId_, trackIndex_);
    if (t) {
        t->removeClip(clip_->id());
        qInfo() << "[AddClipCommand] undo: removed clip" << clip_->id();
    }
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
    if (!tl)
        return;

    // 現在クリップが乗っているトラックを特定して取り出す
    std::shared_ptr<Clip> clip = tl->findClip(clipId_);
    if (!clip)
        return;
    Track* src = nullptr;
    for (int i = 0; i < tl->trackCount(); ++i) {
        Track* t = tl->trackAt(i);
        if (t->clipById(clipId_)) {
            src = t;
            break;
        }
    }
    if (!src)
        return;
    clip = src->takeClip(clipId_);

    // 移動先へ入れる。失敗したら元の位置へ戻し、クリップを失わない。
    Track* dst = tl->trackById(after_.trackId);
    const bool dstOk = dst && dst->acceptsClip(*clip);

    if (dstOk) {
        clip->setRange(after_.range);
        if (dst->insertClip(clip)) {
            qInfo() << "[MoveClipCommand] redo: moved clip" << clipId_
                    << "to track" << dst->id()
                    << "range" << clip->range().start << "->" << clip->range().end();
            return;
        }
        qInfo() << "[MoveClipCommand] redo: dst insert FAILED, restoring";
    } else {
        qInfo() << "[MoveClipCommand] redo: dst invalid/incompatible, restoring";
    }

    clip->setRange(before_.range);
    src->insertClip(clip);
}

void MoveClipCommand::doUndo()
{
    Timeline* tl = project()->timeline();
    if (!tl)
        return;
    auto clip = tl->findClip(clipId_);
    if (!clip)
        return;

    Track* src = nullptr;
    for (int i = 0; i < tl->trackCount(); ++i) {
        Track* t = tl->trackAt(i);
        if (t->clipById(clipId_)) {
            src = t;
            break;
        }
    }
    if (!src)
        return;
    clip = src->takeClip(clipId_);

    clip->setRange(before_.range);
    Track* dst = tl->trackById(before_.trackId);
    if (!dst || !dst->acceptsClip(*clip) || !dst->insertClip(clip)) {
        // 戻せない場合は移動後の位置へ戻す (クリップを失わない)
        clip->setRange(after_.range);
        src->insertClip(clip);
    }
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
