#include "SplitClipCommand.h"
#include "../Clip.h"
#include "../Project.h"
#include "../Timeline.h"
#include "../Track.h"

namespace yave {

// ===========================================================================
//  SplitClipCommand
// ===========================================================================

SplitClipCommand::SplitClipCommand(Project* project, const QUuid& trackId,
                                   const QUuid& clipId, int64_t splitFrame)
    : UndoCommandBase(project, QObject::tr("Split clip"))
    , trackId_(trackId)
    , clipId_(clipId)
    , splitFrame_(splitFrame)
{}

void SplitClipCommand::doRedo()
{
    Track* t = project()->timeline()->trackById(trackId_);
    if (!t)
        return;

    auto original = t->takeClip(clipId_);
    if (!original)
        return;

    if (!left_) {
        // 初回 redo: 分割を実行する
        const TimeRange r = original->range();
        const int64_t   offset = splitFrame_ - r.start;
        if (offset <= 0 || offset >= r.duration) {
            t->insertClip(original);   // 分割位置が不正なら元へ戻す
            return;
        }

        left_  = original->clone();
        right_ = original->clone();
        left_->setRange({r.start, offset});
        right_->setRange({splitFrame_, r.duration - offset});
        right_->shiftSourceOffset(offset);

        // 字幕の場合はテキスト / エフェクトスタックも複製される (clone の責務)。
        // タイプライター等の progress 依存エフェクトは区間が変わるため見た目が変わるが、
        // これは仕様として許容する。
    }

    original_ = original;
    t->insertClip(left_);
    t->insertClip(right_);
}

void SplitClipCommand::doUndo()
{
    Track* t = project()->timeline()->trackById(trackId_);
    if (!t || !original_)
        return;

    t->removeClip(left_ ? left_->id() : QUuid());
    t->removeClip(right_ ? right_->id() : QUuid());
    t->insertClip(original_);
}

// ===========================================================================
//  RippleDeleteCommand
// ===========================================================================

RippleDeleteCommand::RippleDeleteCommand(Project* project,
                                         const std::vector<int>& trackIndices,
                                         const TimeRange& gone)
    : UndoCommandBase(project, QObject::tr("Ripple delete"))
    , trackIndices_(trackIndices)
    , gone_(gone)
{}

void RippleDeleteCommand::doRedo()
{
    Timeline* tl = project()->timeline();
    for (int idx : trackIndices_) {
        Track* t = tl->trackAt(idx);
        if (!t)
            continue;

        for (auto& removed : t->removeClipsIn(gone_))
            removedClips_.push_back({t->id(), removed});

        t->shiftClipsAfter(gone_.start, -gone_.duration);
    }
}

void RippleDeleteCommand::doUndo()
{
    Timeline* tl = project()->timeline();

    // (1) 先に全対象トラックの後続クリップを元の位置へ戻す
    for (int idx : trackIndices_) {
        Track* t = tl->trackAt(idx);
        if (!t)
            continue;
        t->shiftClipsAfter(gone_.start, gone_.duration);
    }

    // (2) 削除したクリップを挿し戻す。range は gone 内に完全収まるので
    //     戻したクリップとの衝突は起きない。
    for (const auto& e : removedClips_) {
        Track* t = tl->trackById(e.trackId);
        if (t)
            t->insertClip(e.clip);
    }
    removedClips_.clear();
}

} // namespace yave
