#pragma once

#include "../TimeRange.h"
#include "../TrackType.h"
#include "UndoCommandBase.h"

#include <QUuid>
#include <memory>
#include <vector>

namespace yave {

class Clip;
class Track;

/// SRT の重なりをどう解決するか
enum class OverlapPolicy
{
    SplitToNewTracks,   ///< 重なるキューを新しいトラックへ振り分ける (既定)
    TrimPrevious,       ///< 前のキューの Out を次のキューの In に合わせる
    SkipOverlapping     ///< 重なるキューを取り込まない
};

/// 字幕 / AI 生成字幕など、既成クリップ群の一括挿入を 1 個の Undo コマンドとして扱う。
/// 500 キューを 1 回の Undo で取り消せることが重要
/// (クリップごとにコマンドを積むと Undo を 500 回押す羽目になる)。
///
/// クリップの生成はコマンド外で行い、このコマンドは配置のみを担う
/// (yave_core が subtitle モジュールに依存しないため)。
class ImportSubtitleCommand : public UndoCommandBase
{
public:
    /// targetTrackIndex == -1 なら新規トラックを作る。
    ImportSubtitleCommand(Project* project,
                          std::vector<std::shared_ptr<Clip>> clips,
                          OverlapPolicy policy,
                          int targetTrackIndex,
                          TrackType trackType,
                          const QString& sourceFileName);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    Track* ensureTrack(int baseIndex, int overflowLevel);

    std::vector<std::shared_ptr<Clip>> clips_;
    OverlapPolicy                      policy_;
    int                                targetTrackIndex_;
    TrackType                          trackType_;

    // Undo 用の記録
    std::vector<int> createdTrackIndices_;
    struct InsertedEntry { QUuid trackId; QUuid clipId; };
    std::vector<InsertedEntry> insertedClips_;
};

} // namespace yave
