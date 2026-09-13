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
    baseTrackIndex_ = -1;

    Timeline* tl = project()->timeline();

    int baseIndex = targetTrackIndex_;
    if (baseIndex >= 0) {
        // 指定トラックが取り込み先の種別 (字幕) と一致しない場合は無視して新規作成。
        const Track* t = tl->trackAt(baseIndex);
        if (!t || t->type() != trackType_)
            baseIndex = -1;
    }
    if (baseIndex < 0) {
        Track* t = tl->appendTrack(trackType_);
        baseIndex = tl->indexOfTrack(t);
        createdTrackIndices_.push_back(baseIndex);
        qInfo() << "[ImportSubtitleCommand] created track index:" << baseIndex;
    }
    baseTrackIndex_ = baseIndex;
    qInfo() << "[ImportSubtitleCommand] redo: baseTrackIndex=" << baseIndex
            << "clipCount=" << clips_.size();

    // 字幕トラックは重なり許容のため、すべてベーストラックへそのまま入れる。
    // それ以外のトラック種別では従来どおり重なりを policy で解決する。
    Track* base = ensureTrack(baseIndex, 0);
    const bool allowOverlaps = base && base->allowOverlaps();

    int64_t prevEnd = std::numeric_limits<int64_t>::min();

    for (const auto& clip : clips_) {
        if (!clip)
            continue;

        const bool overlaps = (clip->range().start < prevEnd);

        Track* target = nullptr;
        if (allowOverlaps) {
            // 重なりを許す: キューをそのまま1本のトラックへ積む (短縮・分割しない)
            target = base;
        } else switch (policy_) {

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
                if (pr.duration > 0) {
                    prev->setRange(pr);
                    // 短縮後の終端を反映し、次の判定を正しくする
                    prevEnd = clip->range().start;
                }
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
        if (!target->insertClip(clip)) {
            qInfo() << "[ImportSubtitleCommand] insertClip FAILED (overlap?) trackId:"
                    << target->id() << "range:" << clip->range().start
                    << "->" << clip->range().end();
            continue;
        }

        insertedClips_.push_back({target->id(), clip->id()});
        prevEnd = prevEnd > clip->range().end() ? prevEnd : clip->range().end();
    }
    qInfo() << "[ImportSubtitleCommand] redo done: inserted=" << insertedClips_.size();
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
