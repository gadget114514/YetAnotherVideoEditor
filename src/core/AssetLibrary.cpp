#include "AssetLibrary.h"

#include <QFileInfo>

namespace yave {

AssetLibrary::AssetLibrary(QObject* parent) : QObject(parent) {}
AssetLibrary::~AssetLibrary() = default;

Asset* AssetLibrary::registerAsset(const QString& absolutePath, Asset::Kind kind)
{
    const QString norm = QDir::fromNativeSeparators(QFileInfo(absolutePath).absoluteFilePath());
    for (auto it = assets_.begin(); it != assets_.end(); ++it) {
        if (it->resolvedAbsolutePath == norm)
            return &it.value();
    }

    Asset a;
    a.id                   = QUuid::createUuid();
    a.kind                 = kind;
    a.resolvedAbsolutePath = norm;
    a.isMissing            = !QFileInfo::exists(norm);
    return addResolvedAsset(a);
}

Asset* AssetLibrary::addResolvedAsset(const Asset& asset)
{
    if (asset.id.isNull())
        return nullptr;
    const QUuid id = asset.id;
    assets_.insert(id, asset);
    emit assetAdded(id);
    return &assets_[id];
}

void AssetLibrary::removeAsset(const QUuid& id)
{
    if (assets_.remove(id)) {
        refCounts_.remove(id);
        proxies_.remove(id);
        emit assetRemoved(id);
    }
}

Asset* AssetLibrary::asset(const QUuid& id) const
{
    auto it = assets_.constFind(id);
    if (it == assets_.cend())
        return nullptr;
    return const_cast<Asset*>(&it.value());
}

Asset* AssetLibrary::findByPath(const QString& normalizedRelativePath) const
{
    for (auto it = assets_.cbegin(); it != assets_.cend(); ++it) {
        if (it->relativePath == normalizedRelativePath)
            return const_cast<Asset*>(&it.value());
    }
    return nullptr;
}} // namespace yave
