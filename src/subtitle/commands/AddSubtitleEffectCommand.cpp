#include "AddSubtitleEffectCommand.h"

#include "../SubtitleClip.h"

#include "../../core/Project.h"
#include "../../core/Timeline.h"

#include <QObject>

namespace yave::subtitle {

namespace {

/// clipId が指す字幕 / タイトルクリップ。別種のクリップなら nullptr。
SubtitleClip* findSubtitleClip(yave::Project* project, const QUuid& clipId)
{
    yave::Timeline* tl = project ? project->timeline() : nullptr;
    if (!tl)
        return nullptr;
    auto clip = tl->findClip(clipId);
    if (!clip)
        return nullptr;
    if (clip->type() != yave::ClipType::Subtitle && clip->type() != yave::ClipType::Title)
        return nullptr;
    return static_cast<SubtitleClip*>(clip.get());
}

} // anonymous namespace

// ===========================================================================
//  AddSubtitleEffectCommand
// ===========================================================================

AddSubtitleEffectCommand::AddSubtitleEffectCommand(yave::Project* project, const QUuid& clipId,
                                                   const QString& effectId, int index)
    : yave::UndoCommandBase(project, QObject::tr("Add effect"))
    , clipId_(clipId)
    , effectId_(effectId)
    , index_(index)
{}

void AddSubtitleEffectCommand::doRedo()
{
    SubtitleClip* clip = findSubtitleClip(project(), clipId_);
    if (!clip)
        return;
    if (index_ < 0)
        index_ = int(clip->effectStack().size());

    clip->insertEffect(index_, SubtitleEffectInstance::create(effectId_));
    added_ = true;
}

void AddSubtitleEffectCommand::doUndo()
{
    if (!added_)
        return;
    if (SubtitleClip* clip = findSubtitleClip(project(), clipId_))
        clip->removeEffect(index_);
    added_ = false;
}

// ===========================================================================
//  RemoveSubtitleEffectCommand
// ===========================================================================

RemoveSubtitleEffectCommand::RemoveSubtitleEffectCommand(yave::Project* project,
                                                         const QUuid& clipId, int index)
    : yave::UndoCommandBase(project, QObject::tr("Remove effect"))
    , clipId_(clipId)
    , index_(index)
{
    if (SubtitleClip* clip = findSubtitleClip(project, clipId)) {
        if (index >= 0 && index < int(clip->effectStack().size())) {
            const auto& inst = clip->effectStack()[size_t(index)];
            effectId_ = inst.effectId;
            pluginId_ = inst.pluginId;
            valid_    = true;
        }
    }
}

void RemoveSubtitleEffectCommand::doRedo()
{
    if (!valid_)
        return;
    if (SubtitleClip* clip = findSubtitleClip(project(), clipId_))
        clip->removeEffect(index_);
}

void RemoveSubtitleEffectCommand::doUndo()
{
    if (!valid_)
        return;
    if (SubtitleClip* clip = findSubtitleClip(project(), clipId_))
        clip->insertEffect(index_, SubtitleEffectInstance::create(effectId_, pluginId_));
}

} // namespace yave::subtitle
