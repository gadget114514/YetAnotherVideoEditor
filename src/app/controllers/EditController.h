#pragma once

#include "../core/Rational.h"
#include "../core/TimeRange.h"

#include <QObject>
#include <QUuid>
#include <QVariantMap>

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

    // ================= インスペクタ (1.7.1 inspector) =================

    /// クリップのプロパティを Undo 可能に変更する。prop は "name" / "start" /
    /// "duration" / "sourceOffset" / "opacity" / "blendMode" / "fadeIn" /
    /// "fadeOut" / "enabled" / "locked" / "gain" / "pan"。
    Q_INVOKABLE void setClipProperty(const QString& clipId, const QString& prop,
                                     const QVariant& value);

    /// 字幕 / タイトルクリップのテキストを Undo 可能に編集する。
    Q_INVOKABLE void setSubtitleText(const QString& clipId, const QString& text);

    /// 字幕 / タイトルクリップのスタイル差分を Undo 可能に編集する。
    /// prop は "fontFamily" / "fillColor" / "fontPointSize" / "anchor" など
    /// SubtitleStyleDiff のフィールド名。value が null ならプリセット値へ戻す。
    Q_INVOKABLE void setSubtitleStyle(const QString& clipId, const QString& prop,
                                      const QVariant& value);

    // ================= 字幕 SRT 取り込み =================

    /// SRT ファイルを読み込み、字幕トラックへ一括で取り込む。
    ///
    /// options:
    ///   overlapPolicy: 0=別トラックへ振り分け(既定) 1=前のキューを短縮 2=重なるものをスキップ
    ///   targetTrackIndex: -1(既定)=末尾に新しい字幕トラックを作る。既存トラック番号を指定するとそこへ
    ///   stylePresetId: 取り込むクリップに割り当てるスタイルプリセット (既定 "default")
    ///   fadeInFrames / fadeOutFrames: 出方 / 消去時のフェード (既定 0)
    ///
    /// 戻り値: ok, importedCount, trackId, trackIndex, warnings(QStringList)。
    Q_INVOKABLE QVariantMap importSrt(const QString& path,
                                      const QVariantMap& options = {});

    /// トラックのプロパティを Undo 可能に変更する。prop は "name" / "gain" /
    /// "pan" / "muted" / "solo" / "opacity" / "blendMode" / "visible" /
    /// "locked" / "height"。
    Q_INVOKABLE void setTrackProperty(const QString& trackId, const QString& prop,
                                      const QVariant& value);

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
