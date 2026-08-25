#pragma once

#include "../core/Rational.h"
#include "../core/TimeRange.h"

#include <QObject>
#include <QUuid>

namespace yave {

class Project;
class Clip;
class Track;

/// 編集操作のエントリポイント。
///
/// すべての操作は QUndoCommand として QUndoStack へ積む (3.2 設計方針)。
/// UI 操作に限らず、AI 生成結果のコミットや SRT 一括取り込みにも適用する。
class EditController : public QObject
{
    Q_OBJECT
public:
    explicit EditController(QObject* parent = nullptr);

    void setProject(Project* project);

    // ================= クリップ編集 =================

    Q_INVOKABLE bool addClip(int trackIndex, const QUuid& trackId, qint64 startFrame,
                             qint64 durationFrames);
    Q_INVOKABLE bool addAssetClip(int trackIndex, const QUuid& trackId, const QString& assetIdStr,
                                  qint64 startFrame, qint64 durationFrames);
    /// トラックの種類に合わせてクリップ種別を自動選択する (右クリック挿入用)。
    Q_INVOKABLE bool addClipToTrack(int trackIndex, const QUuid& trackId,
                                    qint64 startFrame, qint64 durationFrames);
    Q_INVOKABLE void removeClip(const QUuid& clipId);
    Q_INVOKABLE void moveClip(const QUuid& fromTrackId, const QUuid& toTrackId,
                              const QUuid& clipId,
                              qint64 newStart, qint64 newDuration);
    Q_INVOKABLE void trimClip(const QUuid& trackId, const QUuid& clipId,
                              qint64 newSourceOffset,
                              qint64 newStart, qint64 newDuration);
    Q_INVOKABLE bool splitClip(const QUuid& trackId, const QUuid& clipId,
                               qint64 splitFrame);
    Q_INVOKABLE void rippleDelete(const TimeRange& range);

    // ================= ライブラリからのドロップ (1.7.5) =================

    /// ライブラリのアイテムをタイムラインへ落としたときの入口。
    ///
    /// カテゴリ別の分岐と Undo コマンド発行をここに集約し、QML には
    /// 「どこへ落ちたか」だけを伝えさせる (編集ロジックを QML に持たせない)。
    ///
    /// payloadJson : LibraryItemsModel の dragPayload
    /// targetClipId: クリップの上へ落ちた場合のみ非 null
    Q_INVOKABLE bool dropLibraryItem(const QString& payloadJson, const QUuid& trackId,
                                     qint64 frame, const QUuid& targetClipId = {});

    /// 直前の dropLibraryItem が失敗した理由 (翻訳済み)。成功時は空。
    Q_INVOKABLE QString lastDropError() const { return lastDropError_; }

    /// ドラッグ中のハイライト判定に使う。実際の編集は行わない。
    Q_INVOKABLE bool canDropOnClip(const QString& category) const;
    Q_INVOKABLE bool canDropOnTrack(const QString& category, const QUuid& trackId) const;

    /// frame の近くにクリップ境界があればそのフレームを返す。無ければ -1。
    Q_INVOKABLE qint64 clipBoundaryNear(const QUuid& trackId, qint64 frame,
                                        qint64 toleranceFrames) const;

    // ================= トラック =================

    Q_INVOKABLE int  addTrack(const QString& type, int index);
    Q_INVOKABLE void removeTrack(const QUuid& trackId);
    Q_INVOKABLE void reorderTracks(int from, int to);

    /// Undo / Redo。QML のメニューから呼ぶ。
    Q_INVOKABLE void undo() const;
    Q_INVOKABLE void redo() const;

    /// ドラッグ中のスナップ候補 (3.8.3)
    Q_INVOKABLE QVariantList snapCandidates(qint64 visibleStart, qint64 visibleEnd) const;

signals:
    void editChanged();

    /// ドロップできなかったときの通知。コンソールパネルへ出す (1.7.5)。
    void dropRejected(const QString& reason);

private:
    Project* project_ = nullptr;
    QString  lastDropError_;
};

} // namespace yave
