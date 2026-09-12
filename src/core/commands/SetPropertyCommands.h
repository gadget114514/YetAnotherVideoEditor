#pragma once

#include "../BlendMode.h"
#include "UndoCommandBase.h"

#include <QUuid>
#include <QVariant>

namespace yave {

/// クリップの単一プロパティを Undo 可能に変更する汎用コマンド。
/// 適用後は Timeline::clipChanged(trackId, clipId) を発行する。
enum class ClipProperty
{
    Name, Start, Duration, SourceOffset, Opacity, BlendMode,
    FadeIn, FadeOut, Enabled, Locked, Gain, Pan
};

class SetClipPropertyCommand : public UndoCommandBase
{
public:
    SetClipPropertyCommand(Project* project, const QUuid& clipId,
                           ClipProperty prop, const QVariant& value);

protected:
    void doRedo() override;
    void doUndo() override;
    bool affectsAudioGraph() const override
    {
        return prop_ == ClipProperty::Gain || prop_ == ClipProperty::Pan;
    }

private:
    void apply(const QVariant& v);

    QUuid        clipId_;
    ClipProperty prop_;
    QVariant     old_;
    QVariant     new_;
};

/// トラックの単一プロパティを Undo 可能に変更する汎用コマンド。
/// 適用後は Timeline::trackChanged(index) を発行する。
enum class TrackProperty
{
    Name, Gain, Pan, Muted, Solo, Opacity, BlendMode, Visible, Locked, UiHeight
};

class SetTrackPropertyCommand : public UndoCommandBase
{
public:
    SetTrackPropertyCommand(Project* project, const QUuid& trackId,
                            TrackProperty prop, const QVariant& value);

protected:
    void doRedo() override;
    void doUndo() override;
    bool affectsAudioGraph() const override
    {
        return prop_ == TrackProperty::Gain || prop_ == TrackProperty::Pan
            || prop_ == TrackProperty::Muted || prop_ == TrackProperty::Solo;
    }

private:
    void apply(const QVariant& v);

    QUuid         trackId_;
    TrackProperty prop_;
    QVariant      old_;
    QVariant      new_;
};

} // namespace yave