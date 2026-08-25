#pragma once

#include "../core/Project.h"
#include "../core/Rational.h"
#include "../core/TimeRange.h"

#include <QObject>
#include <QString>

namespace yave {

class ProjectController : public QObject
{
    Q_OBJECT
public:
    explicit ProjectController(QObject* parent = nullptr);

    Project* project() const { return project_.get(); }

    // ================= 新規 / 開く / 保存 =================

    Q_INVOKABLE void newProject(const QString& name = {});
    Q_INVOKABLE bool open(const QString& path);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool saveAs(const QString& path);

    /// アセットをパスまたはURLから登録し、メディア情報プローブを行ってアセットIDを返す
    Q_INVOKABLE QString registerAsset(const QString& absolutePathOrUrl);

    /// ライブラリのフォルダへアセットを入れる (1.7.5)。
    /// 取り込み直後に「いま開いているフォルダ」へ入れるために QML から呼ぶ。
    Q_INVOKABLE void assignAssetToFolder(const QString& assetId, const QString& folderId);

    /// 自動保存の復元。前回異常終了時に .autosave が残っていた場合に呼ぶ。
    Q_INVOKABLE bool restoreAutosave();

    Q_INVOKABLE QString projectPath() const { return projectPath_; }
    Q_INVOKABLE bool isModified() const;

signals:
    void projectOpened(const QString& path);
    void projectSaved(const QString& path);
    void projectClosed();
    void modifiedChanged();

private:
    std::unique_ptr<Project> project_;
    QString projectPath_;
};

} // namespace yave
