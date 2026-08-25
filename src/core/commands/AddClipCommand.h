#pragma once

#include "../TimeRange.h"
#include "UndoCommandBase.h"

#include <QUuid>
#include <memory>

namespace yave {

class Track;
class Clip;

/// クリップ挿入。挿入できなかった場合 (重なり) は no-op。
class AddClipCommand : public UndoCommandBase
{
public:
    /// trackIndex は実行時解決 (トラックが先に作られるケースに対応)。
    /// -1 の場合は末尾トラックへ。trackId が既知ならそちらを優先する。
    AddClipCommand(Project* project, const QUuid& trackId, int trackIndex,
                   const std::shared_ptr<Clip>& clip);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    QUuid                 trackId_;
    int                   trackIndex_;
    std::shared_ptr<Clip> clip_;
    bool                  inserted_ = false;
};

/// クリップ削除。
class RemoveClipCommand : public UndoCommandBase
{
public:
    RemoveClipCommand(Project* project, const QUuid& trackId, const QUuid& clipId);

protected:
    void doRedo() override;
    void doUndo() override;

private:
    QUuid                 trackId_;
    QUuid                 clipId_;
    std::shared_ptr<Clip> removed_;
};

/// クリップ移動 (同一 / 別トラック + 時間移動)。
/// 自動マージ: 同一クリップの連続ドラッグを 1 コマンドにまとめる。
class MoveClipCommand : public UndoCommandBase
{
public:
    MoveClipCommand(Project* project,
                    const QUuid& fromTrackId, const QUuid& toTrackId,
                    const QUuid& clipId,
                    const TimeRange& newRange);

    int  id() const override { return IdMoveClip; }
    bool mergeWith(const QUndoCommand* other) override;

protected:
    void doRedo() override;
    void doUndo() override;

private:
    struct State
    {
        QUuid     trackId;
        TimeRange range;
    };

    QUuid  clipId_;
    State  before_;   ///< undo 用
    State  after_;    ///< redo 後の状態
    bool   firstRun_ = true;
};

/// クリップトリム (In / Out 変更)。自動マージ対応。
class TrimClipCommand : public UndoCommandBase
{
public:
    TrimClipCommand(Project* project, const QUuid& trackId, const QUuid& clipId,
                    int64_t newSourceOffset, const TimeRange& newRange);

    int  id() const override { return IdTrimClip; }
    bool mergeWith(const QUndoCommand* other) override;

protected:
    void doRedo() override;
    void doUndo() override;

private:
    QUuid   trackId_;
    QUuid   clipId_;
    int64_t oldOffset_ = 0;
    int64_t newOffset_ = 0;
    TimeRange oldRange_;
    TimeRange newRange_;
    bool      firstRun_ = true;
};

} // namespace yave
