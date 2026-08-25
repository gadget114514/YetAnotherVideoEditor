#include "ImportSubtitleCommand.h"
#include "../Clip.h"
#include "../Project.h"
#include "../Timeline.h"
#include "../Track.h"

#include <limits>

namespace yave {

ImportSubtitleCommand::ImportSubtitleCommand(
        Project* project,
        std::vector<std::shared_ptr<Clip>> clips,
        OverlapPolicy policy,
        int targetTrackIndex,
        TrackType trackType,
        const QString& sourceFileName)
    : UndoCommandBase(project,
                      QObject::tr("Import %n subtitle cue(s) from %1", "",
                                  int(clips.size())).arg(sourceFileName))
    , clips_(std::move(clips))
    , policy_(policy)
    , targetTrackIndex_(targetTrackIndex)
    , trackType_(trackType)
{}

Track* ImportSubtitleCommand::ensureTrack(int baseIndex, int overflowLevel)
{
    Timeline* tl = project()->timeline();
    // overflowLevel 0 = ベーストラック、1 以上 = 重なり用の追加トラック
    const int wanted = baseIndex + overflowLevel;
    while (tl->trackCount() <= wanted) {
        Track* t = tl->appendTrack(trackType_);
        createdTrackIndices_.push_back(tl->indexOfTrack(t));
    }
    return tl->trackAt(wanted);
}

void ImportSubtitleCommand::doRedo()
{
    createdTrackIndices_.clear();
    insertedClips_.clear();

    Timeline* tl = project()->timeline();

    int baseIndex = targetTrackIndex_;
    if (baseIndex < 0) {
        Track* t = tl->appendTrack(trackType_);
        baseIndex = tl->indexOfTrack(t);
        createdTrackIndices_.push_back(baseIndex);
    }

    int64_t prevEnd = std::numeric_limits<int64_t>::min();

    for (const auto& clip : clips_) {
        if (!clip)
            continue;

        const bool overlaps = (clip->range().start < prevEnd);

        Track* target = nullptr;
        switch (policy_) {

        case OverlapPolicy::SplitToNewTracks:
            // 無限レイヤーの利点をそのまま使う。
            // 重なったキューは 1 段上のトラックへ置く。さらに重なれば 2 段上へ。
            for (int level = 0;; ++level) {
                Track* t = ensureTrack(baseIndex, level);
                if (t->clipsIn(clip->range()).empty()) {
                    target = t;
                    break;
                }
            }
            break;

        case OverlapPolicy::TrimPrevious: {
            target = ensureTrack(baseIndex, 0);
            auto clipsOnTrack = target->clips();
            if (overlaps && !clipsOnTrack.empty()) {
                auto& prev = clipsOnTrack.back();
                TimeRange pr = prev->range();
                pr.duration = clip->range().start - pr.start;
                if (pr.duration > 0)
                    prev->setRange(pr);
            }
            break;
        }

        case OverlapPolicy::SkipOverlapping:
            if (overlaps)
                continue;
            target = ensureTrack(baseIndex, 0);
            break;
        }

        if (!target)
            continue;
        if (!target->insertClip(clip))
            continue;

        insertedClips_.push_back({target->id(), clip->id()});
        prevEnd = prevEnd > clip->range().end() ? prevEnd : clip->range().end();
    }
}

void ImportSubtitleCommand::doUndo()
{
    Timeline* tl = project()->timeline();

    // (1) 挿入したクリップを除去する
    for (const auto& entry : insertedClips_) {
        if (Track* t = tl->trackById(entry.trackId))
            t->removeClip(entry.clipId);
    }
    insertedClips_.clear();

    // (2) 作成したトラックを削除する。
    //     index が大きいものから消さないと、削除のたびに後続の index がずれる。
    std::sort(createdTrackIndices_.begin(), createdTrackIndices_.end(), std::greater<int>());
    for (int idx : createdTrackIndices_)
        (void)tl->takeTrack(idx);
    createdTrackIndices_.clear();
}

} // namespace yave
