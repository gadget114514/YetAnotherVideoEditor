#pragma once

#include "../../core/AssetLibrary.h"

#include <QAbstractListModel>
#include <QUuid>
#include <vector>

namespace yave {

class Project;

class AssetListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        NameRole,
        PathRole,
        KindRole,
        DurationRole,
        ResolutionRole,
    };

    explicit AssetListModel(QObject* parent = nullptr);

    void setProject(Project* project);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private slots:
    void rebuild();

private:
    Project* project_ = nullptr;
    std::vector<QUuid> assetIds_;
};

} // namespace yave
