#pragma once

#include <QObject>
#include <QString>
#include <QUuid>
#include <QVariantMap>

namespace yave {

class Project;

/// タイムライン上の選択状態 (クリップ / トラック) を保持する UI 層モデル。
///
/// 選択状態は Timeline に持たせず、ここ (Controller 層) で管理する (3.8)。
/// インスペクタは clipInfo() / trackInfo() を読んで表示・編集する。
/// 編集コマンドによる変更 (Timeline::structureChanged) で selectionChanged を
/// 再発行し、インスペクタの表示を追従させる。
class SelectionModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasClip READ hasClip NOTIFY selectionChanged)
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY selectionChanged)
    Q_PROPERTY(QString clipId READ clipIdStr NOTIFY selectionChanged)
    Q_PROPERTY(QString trackId READ trackIdStr NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap clipInfo READ clipInfo NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap trackInfo READ trackInfo NOTIFY selectionChanged)

public:
    explicit SelectionModel(QObject* parent = nullptr);

    void setProject(Project* project);

    Q_INVOKABLE void selectClip(const QString& trackId, const QString& clipId);
    Q_INVOKABLE void selectTrack(const QString& trackId);
    Q_INVOKABLE void clear();

    bool hasClip() const { return !clipId_.isNull(); }
    bool hasTrack() const { return !trackId_.isNull(); }
    QString clipIdStr() const { return clipId_.toString(QUuid::WithoutBraces); }
    QString trackIdStr() const { return trackId_.toString(QUuid::WithoutBraces); }

    /// 選択中クリップの表示用プロパティ (空なら空マップ)。
    Q_INVOKABLE QVariantMap clipInfo() const;
    /// 選択中トラックの表示用プロパティ (空なら空マップ)。
    Q_INVOKABLE QVariantMap trackInfo() const;

signals:
    void selectionChanged();

private:
    void onStructureChanged();

    Project* project_ = nullptr;
    QUuid    clipId_;
    QUuid    trackId_;
};

} // namespace yave