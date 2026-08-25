#pragma once

#include "UndoCommandBase.h"

#include <QUuid>
#include <memory>
#include <vector>

namespace yave {

class Clip;

/// AI 生成結果のコミット。プレースホルダを実クリップへ置換する。
/// 生成結果の反映も Undo 可能でなければならない (3.2 設計方針)。
class CommitGeneratedAssetCommand : public UndoCommandBase
{
public:
    CommitGeneratedAssetCommand(Project* project,
                                const QUuid& targetTrackId,
                                std::shared_ptr<Clip> placeholderClip,
                                std::vector<std::shared_ptr<Clip>> generatedClips);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    QUuid                              targetTrackId_;
    std::shared_ptr<Clip>              placeholder_;
    std::shared_ptr<Clip>              removedPlaceholder_;   ///< redo 時に取り除いた実体
    bool                               placeholderRemoved_ = false;
    std::vector<std::shared_ptr<Clip>> generated_;
};

} // namespace yave
