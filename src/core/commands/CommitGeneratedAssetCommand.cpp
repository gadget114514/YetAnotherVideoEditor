#include "CommitGeneratedAssetCommand.h"
#include "../Clip.h"
#include "../Project.h"
#include "../Timeline.h"
#include "../Track.h"

namespace yave {

CommitGeneratedAssetCommand::CommitGeneratedAssetCommand(
        Project* project,
        const QUuid& targetTrackId,
        std::shared_ptr<Clip> placeholderClip,
        std::vector<std::shared_ptr<Clip>> generatedClips)
    : UndoCommandBase(project, QObject::tr("Commit AI generated asset"))
    , targetTrackId_(targetTrackId)
    , placeholder_(std::move(placeholderClip))
    , generated_(std::move(generatedClips))
{}

void CommitGeneratedAssetCommand::doRedo()
{
    Timeline* tl = project()->timeline();
    Track* t = tl->trackById(targetTrackId_);
    if (!t && tl->trackCount() > 0)
        t = tl->trackAt(tl->trackCount() - 1);
    if (!t)
        return;

    // プレースホルダを外す (Undo 用に保持し続ける)
    if (placeholder_) {
        if (!placeholderRemoved_)
            removedPlaceholder_ = t->takeClip(placeholder_->id());
        else
            removedPlaceholder_.reset();
        placeholderRemoved_ = true;
    }

    for (const auto& c : generated_)
        t->insertClip(c);
}

void CommitGeneratedAssetCommand::doUndo()
{
    Timeline* tl = project()->timeline();
    Track* t = tl->trackById(targetTrackId_);
    if (!t && tl->trackCount() > 0)
        t = tl->trackAt(tl->trackCount() - 1);
    if (!t)
        return;

    for (const auto& c : generated_)
        t->takeClip(c->id());

    // プレースホルダを戻す
    if (removedPlaceholder_)
        t->insertClip(removedPlaceholder_);
}

} // namespace yave
