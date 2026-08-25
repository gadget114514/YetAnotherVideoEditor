#pragma once

#include "../TimeRange.h"
#include "UndoCommandBase.h"

#include <QUuid>
#include <memory>
#include <vector>

namespace yave {

class Clip;
class Track;

/// クリップ分割。3.8.1 参照。
///
/// Undo 用に元クリップを保持し、Redo/Undo 往復で左右の新しいクリップの
/// id が変わらないよう clone を 1 回だけ行う。
class SplitClipCommand : public UndoCommandBase
{
public:
    SplitClipCommand(Project* project, const QUuid& trackId,
                     const QUuid& clipId, int64_t splitFrame);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    QUuid                 trackId_;
    QUuid                 clipId_;
    int64_t               splitFrame_;

    std::shared_ptr<Clip> original_;   ///< Undo 用に保持
    std::shared_ptr<Clip> left_;
    std::shared_ptr<Clip> right_;
};

/// リップル削除。削除した区間より後ろのクリップを詰める。3.8.2 参照。
class RippleDeleteCommand : public UndoCommandBase
{
public:
    /// trackIndices は対象トラック (全トラック連動モードでは全トラック)。
    RippleDeleteCommand(Project* project, const std::vector<int>& trackIndices,
                        const TimeRange& gone);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    struct RemovedEntry
    {
        QUuid                 trackId;
        std::shared_ptr<Clip> clip;
    };

    std::vector<int>     trackIndices_;
    TimeRange            gone_;
    std::vector<RemovedEntry> removedClips_;
};

} // namespace yave
