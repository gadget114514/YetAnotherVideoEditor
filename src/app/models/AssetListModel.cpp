#include "AssetListModel.h"
#include "../../core/Project.h"
#include "../../core/AssetLibrary.h"
#include <QFileInfo>

namespace yave {

AssetListModel::AssetListModel(QObject* parent) : QAbstractListModel(parent) {}

void AssetListModel::setProject(Project* project)
{
    beginResetModel();
    if (project_ && project_->assets()) {
        project_->assets()->disconnect(this);
    }
    project_ = project;
    assetIds_.clear();
    if (project_ && project_->assets()) {
        auto* lib = project_->assets();
        QList<QUuid> ids = lib->allIds();
        for (const auto& id : ids) {
            assetIds_.push_back(id);
        }
        
        connect(lib, &AssetLibrary::assetAdded, this, &AssetListModel::rebuild);
        connect(lib, &AssetLibrary::assetRemoved, this, &AssetListModel::rebuild);
    }
    endResetModel();
}

void AssetListModel::rebuild()
{
    beginResetModel();
    assetIds_.clear();
    if (project_ && project_->assets()) {
        QList<QUuid> ids = project_->assets()->allIds();
        for (const auto& id : ids) {
            assetIds_.push_back(id);
        }
    }
    endResetModel();
}

int AssetListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !project_)
        return 0;
    return int(assetIds_.size());
}

QVariant AssetListModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= int(assetIds_.size()) || !project_ || !project_->assets())
        return {};

    const QUuid& id = assetIds_[index.row()];
    const Asset* a = project_->assets()->asset(id);
    if (!a)
        return {};

    switch (role) {
    case IdRole:       return a->id.toString(QUuid::WithoutBraces);
    case NameRole:     return QFileInfo(a->resolvedAbsolutePath).fileName();
    case PathRole:     return a->resolvedAbsolutePath;
    case KindRole:     switch (a->kind) {
                       case Asset::Kind::Video:     return "video";
                       case Asset::Kind::Audio:     return "audio";
                       case Asset::Kind::Image:     return "image";
                       case Asset::Kind::Generated: return "generated";
                       }
                       break;
    case DurationRole: return qint64(a->durationFrames);
    case ResolutionRole: return a->resolution;
    }
    return {};
}

QHash<int, QByteArray> AssetListModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { IdRole,         "assetId" },
        { NameRole,       "name" },
        { PathRole,       "path" },
        { KindRole,       "kind" },
        { DurationRole,   "duration" },
        { ResolutionRole, "resolution" },
    };
    return roles;
}

} // namespace yave
