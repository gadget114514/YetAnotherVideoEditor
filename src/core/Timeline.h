#pragma once

#include "Rational.h"
#include "RenderSnapshot.h"
#include "TimeRange.h"
#include "TrackType.h"

#include <QSize>
#include <QObject>
#include <QUuid>

#include <memory>
#include <vector>

namespace yave {

class Project;
class Track;
class Clip;

/// 無限レイヤー (トラック数無制限) を保持するコンテナ。
///
/// tracks_ のインデックスがそのまま Z オーダーになる。
///   index 0        = 最背面
///   index size()-1 = 最前面
/// 別途 zOrder フィールドは持たない (二重管理を避けるため)。
///
/// スレッド安全性:
///   構造を変更してよいのは UI スレッドのみ。
///   Render / Audio スレッドは buildSnapshot() の結果 (値のコピー) 越しにのみ参照する。
class Timeline : public QObject
{
    Q_OBJECT
public:
    explicit Timeline(Project* project, QObject* parent = nullptr);
    ~Timeline() override;

    Project* project() const { return project_; }

    // ================= トラック操作 =================

    /// 末尾 (最背面から数えて最前面) に追加する。既定名は type に応じて自動生成。
    Track* appendTrack(TrackType type, const QString& name = {});

    /// 指定位置に挿入する。index == size() なら末尾。
    Track* insertTrack(int index, TrackType type, const QString& name = {});

    /// 所有権ごと取り出す。Undo コマンドが保持するために使う。
    std::unique_ptr<Track> takeTrack(int index);
    std::unique_ptr<Track> takeTrackById(const QUuid& id);

    /// takeTrack で取り出したトラックを指定位置へ戻す。
    void reinsertTrack(int index, std::unique_ptr<Track> track);

    /// 並び替え。Z オーダーが変わる。
    void moveTrack(int from, int to);

    int    trackCount() const { return int(tracks_.size()); }
    Track* trackAt(int index) const;
    Track* trackById(const QUuid& id) const;
    int    indexOfTrack(const Track* t) const;
    int    indexOfTrack(const QUuid& id) const;

    /// 型でフィルタした一覧
    std::vector<Track*> tracksOfType(TrackType type) const;

    // ================= クリップ横断操作 =================

    /// 全トラックから id で探す
    std::shared_ptr<Clip> findClip(const QUuid& clipId, Track** ownerOut = nullptr) const;

    /// すべてのトラックの最大 end()
    int64_t duration() const;

    /// 指定範囲でのスナップ候補 (全対象トラックのクリップ境界)
    std::vector<int64_t> snapCandidates(const TimeRange& visibleRange,
                                        const std::vector<int>& visibleTrackIndices) const;

    // ================= AI プレースホルダ =================

    void insertPlaceholder(int trackIndex, const std::shared_ptr<Clip>& placeholder);
    std::shared_ptr<Clip> takePlaceholder(const QUuid& taskId);
    void restorePlaceholder(const std::shared_ptr<Clip>& placeholder);
    void updatePlaceholderProgress(const QUuid& taskId, double progress);

    // ================= レンダリング =================

    Rational timebase() const { return timebase_; }
    void     setTimebase(const Rational& tb);

    QSize canvasSize() const { return canvasSize_; }
    void  setCanvasSize(const QSize& s);

    /// Render スレッドへ渡すための不変スナップショットを作る。
    /// UI スレッドから毎フレーム呼ばれる。O(トラック数 * log(クリップ数))。
    RenderSnapshot buildSnapshot(int64_t frame) const;

    /// 変更のたびにインクリメントされる。キャッシュ無効化判定に使う。
    uint64_t revision() const { return revision_; }

signals:
    void trackInserted(int index);
    void trackRemoved(int index);
    void trackMoved(int from, int to);
    void trackChanged(int index);
    void clipInserted(const QUuid& trackId, const QUuid& clipId);
    void clipRemoved(const QUuid& trackId, const QUuid& clipId);
    void clipChanged(const QUuid& trackId, const QUuid& clipId);
    void structureChanged();          ///< 音声グラフの再構築が必要になる変更
    void durationChanged(int64_t newDuration);

private:
    void bumpRevision();
    void notifyClipCountChanged();

    Project*                            project_ = nullptr;
    std::vector<std::unique_ptr<Track>> tracks_;     ///< index = Z オーダー
    Rational                            timebase_{1001, 60000};
    QSize                               canvasSize_{3840, 2160};
    uint64_t                            revision_ = 0;
    int64_t                             lastDuration_ = -1;
};

} // namespace yave
