#pragma once

#include "../../core/commands/UndoCommandBase.h"

#include <QString>
#include <QUuid>

namespace yave::subtitle {

/// 字幕 / タイトルクリップのエフェクトスタックへ 1 段追加する (6章)。
///
/// core/commands ではなく subtitle 側に置く理由: SubtitleClip の API を
/// 直接触る必要があり、yave_core は yave_subtitle に依存できないため。
class AddSubtitleEffectCommand : public yave::UndoCommandBase
{
public:
    AddSubtitleEffectCommand(yave::Project* project, const QUuid& clipId,
                             const QString& effectId, int index = -1);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    QUuid   clipId_;
    QString effectId_;
    int     index_ = -1;
    bool    added_ = false;
};

/// エフェクトを 1 段外す。
class RemoveSubtitleEffectCommand : public yave::UndoCommandBase
{
public:
    RemoveSubtitleEffectCommand(yave::Project* project, const QUuid& clipId, int index);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    QUuid   clipId_;
    int     index_    = -1;
    QString effectId_;      ///< undo で作り直すための永続データ
    QString pluginId_;
    bool    valid_    = false;
};

} // namespace yave::subtitle
