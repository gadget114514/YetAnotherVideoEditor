#pragma once

#include "UndoCommandBase.h"
#include "../TrackType.h"

#include <QUuid>
#include <memory>

namespace yave {

class Timeline;
class Track;

/// トラック追加。Undo で取り除き、Redo で同じインスタンスを戻す。
/// 同じインスタンスを再利用するため、Undo/Redo 往復で id が変わらない。
class AddTrackCommand : public UndoCommandBase
{
public:
    AddTrackCommand(Project* project, TrackType type, int index,
                    const QString& name = {});

protected:
    void doRedo() override;
    void doUndo() override;

private:
    TrackType              type_;
    int                    index_;
    QString                name_;
    QUuid                  trackId_;         ///< 初回 redo で確定する
    std::unique_ptr<Track> created_;   ///< 初回 redo 以降はここに保持される
};

/// トラック削除。Undo で元の位置へ戻す。
class RemoveTrackCommand : public UndoCommandBase
{
public:
    explicit RemoveTrackCommand(Project* project, const QUuid& trackId);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    QUuid                  trackId_;
    std::unique_ptr<Track> removed_;
    int                    originalIndex_ = -1;
};

/// トラック並び替え (Z オーダー変更)。
class ReorderTrackCommand : public UndoCommandBase
{
public:
    ReorderTrackCommand(Project* project, int from, int to);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    int from_;
    int to_;
    /// std::rotate の性質上、moveTrack(to, from) では戻らないケースがあるため
    /// 元の index を保持して復元する。
    int originalFromIndex_ = -1;
};

} // namespace yave
