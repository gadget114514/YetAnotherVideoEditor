#include "SetPropertyCommands.h"

#include "../AudioClip.h"
#include "../Project.h"
#include "../Timeline.h"
#include "../Track.h"

#include <QObject>

namespace yave {

namespace {

Clip* resolveClip(Project* project, const QUuid& clipId, Track** ownerOut = nullptr)
{
    Timeline* tl = project ? project->timeline() : nullptr;
    if (!tl)
        return nullptr;
    return tl->findClip(clipId, ownerOut).get();
}

} // anonymous namespace

// ===========================================================================
//  SetClipPropertyCommand
// ===========================================================================

SetClipPropertyCommand::SetClipPropertyCommand(Project* project, const QUuid& clipId,
                                               ClipProperty prop, const QVariant& value)
    : UndoCommandBase(project, QObject::tr("Change clip property"))
    , clipId_(clipId)
    , prop_(prop)
    , new_(value)
{
    const Clip* c = resolveClip(project, clipId);
    if (!c)
        return;
    switch (prop_) {
    case ClipProperty::Name:         old_ = c->name(); break;
    case ClipProperty::Start:        old_ = qint64(c->range().start); break;
    case ClipProperty::Duration:     old_ = qint64(c->range().duration); break;
    case ClipProperty::SourceOffset: old_ = qint64(c->sourceOffset()); break;
    case ClipProperty::Opacity:      old_ = c->opacity(); break;
    case ClipProperty::BlendMode:    old_ = int(c->blendMode()); break;
    case ClipProperty::FadeIn:       old_ = qint64(c->fadeInFrames()); break;
    case ClipProperty::FadeOut:      old_ = qint64(c->fadeOutFrames()); break;
    case ClipProperty::Enabled:      old_ = c->isEnabled(); break;
    case ClipProperty::Locked:       old_ = c->isLocked(); break;
    case ClipProperty::Gain:
        if (const auto* ac = dynamic_cast<const AudioClip*>(c))
            old_ = ac->gain();
        break;
    case ClipProperty::Pan:
        if (const auto* ac = dynamic_cast<const AudioClip*>(c))
            old_ = ac->pan();
        break;
    }
}

void SetClipPropertyCommand::apply(const QVariant& v)
{
    Track* owner = nullptr;
    Clip* c = resolveClip(project(), clipId_, &owner);
    if (!c || !owner)
        return;

    switch (prop_) {
    case ClipProperty::Name:  c->setName(v.toString()); break;
    case ClipProperty::Start: {
        auto r = c->range();
        r.start = v.toLongLong();
        c->setRange(r);
        owner->resort();      ///< ソート順を復元 (不変条件 1)
        break;
    }
    case ClipProperty::Duration: {
        auto r = c->range();
        r.duration = std::max<int64_t>(1, v.toLongLong());
        c->setRange(r);
        break;
    }
    case ClipProperty::SourceOffset: c->setSourceOffset(v.toLongLong()); break;
    case ClipProperty::Opacity:      c->setOpacity(v.toDouble()); break;
    case ClipProperty::BlendMode:    c->setBlendMode(BlendMode(v.toInt())); break;
    case ClipProperty::FadeIn:       c->setFadeInFrames(v.toLongLong()); break;
    case ClipProperty::FadeOut:      c->setFadeOutFrames(v.toLongLong()); break;
    case ClipProperty::Enabled:      c->setEnabled(v.toBool()); break;
    case ClipProperty::Locked:       c->setLocked(v.toBool()); break;
    case ClipProperty::Gain:
        if (auto* ac = dynamic_cast<AudioClip*>(c))
            ac->setGain(v.toDouble());
        break;
    case ClipProperty::Pan:
        if (auto* ac = dynamic_cast<AudioClip*>(c))
            ac->setPan(v.toDouble());
        break;
    }

    if (project() && project()->timeline())
        emit project()->timeline()->clipChanged(owner->id(), clipId_);
}

void SetClipPropertyCommand::doRedo() { apply(new_); }
void SetClipPropertyCommand::doUndo() { apply(old_); }

// ===========================================================================
//  SetTrackPropertyCommand
// ===========================================================================

SetTrackPropertyCommand::SetTrackPropertyCommand(Project* project, const QUuid& trackId,
                                                 TrackProperty prop, const QVariant& value)
    : UndoCommandBase(project, QObject::tr("Change track property"))
    , trackId_(trackId)
    , prop_(prop)
    , new_(value)
{
    Timeline* tl = project ? project->timeline() : nullptr;
    const Track* t = tl ? tl->trackById(trackId) : nullptr;
    if (!t)
        return;
    switch (prop_) {
    case TrackProperty::Name:      old_ = t->name(); break;
    case TrackProperty::Gain:      old_ = t->gain(); break;
    case TrackProperty::Pan:       old_ = t->pan(); break;
    case TrackProperty::Muted:     old_ = t->isMuted(); break;
    case TrackProperty::Solo:      old_ = t->isSolo(); break;
    case TrackProperty::Opacity:   old_ = t->opacity(); break;
    case TrackProperty::BlendMode: old_ = int(t->blendMode()); break;
    case TrackProperty::Visible:   old_ = t->isVisible(); break;
    case TrackProperty::Locked:    old_ = t->isLocked(); break;
    case TrackProperty::UiHeight:  old_ = t->uiHeight(); break;
    }
}

void SetTrackPropertyCommand::apply(const QVariant& v)
{
    Timeline* tl = project() ? project()->timeline() : nullptr;
    Track* t = tl ? tl->trackById(trackId_) : nullptr;
    if (!t || !tl)
        return;

    switch (prop_) {
    case TrackProperty::Name:      t->setName(v.toString()); break;
    case TrackProperty::Gain:      t->setGain(v.toDouble()); break;
    case TrackProperty::Pan:       t->setPan(v.toDouble()); break;
    case TrackProperty::Muted:     t->setMuted(v.toBool()); break;
    case TrackProperty::Solo:      t->setSolo(v.toBool()); break;
    case TrackProperty::Opacity:   t->setOpacity(v.toDouble()); break;
    case TrackProperty::BlendMode: t->setBlendMode(BlendMode(v.toInt())); break;
    case TrackProperty::Visible:   t->setVisible(v.toBool()); break;
    case TrackProperty::Locked:    t->setLocked(v.toBool()); break;
    case TrackProperty::UiHeight:  t->setUiHeight(v.toInt()); break;
    }

    emit tl->trackChanged(tl->indexOfTrack(t));
}

void SetTrackPropertyCommand::doRedo() { apply(new_); }
void SetTrackPropertyCommand::doUndo() { apply(old_); }

} // namespace yave