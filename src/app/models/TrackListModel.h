#pragma once

#include "../core/TrackType.h"

#include <QAbstractListModel>
#include <QUuid>

#include <vector>

namespace yave {

class Project;
class Timeline;
class Track;
class Clip;

/// タイムラインのトラック一覧モデル (QML TimelineView 用)。
///
/// 行 = トラック (Z オーダー順)。クリップは ClipListModel が担当する。
class TrackListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles
    {
        TrackIdRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        ColorRole,
        HeightRole,
        VisibleRole,
        LockedRole,
        MutedRole,
        SoloRole,
        GainRole,
        PanRole,
        OpacityRole,
        BlendModeRole,
    };

    explicit TrackListModel(QObject* parent = nullptr);

    void setProject(Project* project);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void moveTrack(int from, int to);
    Q_INVOKABLE QObject* clipModelProvider(int row);

    Track* trackAt(int row) const;

private:
    void rebuild();

    Project* project_ = nullptr;
};

/// 1 トラック上のクリップ一覧モデル。
class ClipListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles
    {
        ClipIdRole = Qt::UserRole + 1,
        StartRole,
        DurationRole,
        EndRole,
        NameRole,
        TypeRole,
        EnabledRole,
        LockedRole,
        OpacityRole,
        FadeInRole,
        FadeOutRole,
        TextPreviewRole,      ///< 字幕: 先頭 20 文字
        HasEffectsRole,
        MissingEffectsRole,
        GeneratedByAiRole,
        ProgressRole,         ///< AI プレースホルダ用
    };

    explicit ClipListModel(QObject* parent = nullptr);

    void setTrack(Timeline* timeline, Track* track);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    Timeline* timeline_ = nullptr;
    Track* track_ = nullptr;
    std::vector<std::shared_ptr<Clip>> clips_;
};

} // namespace yave
