#include "LibraryItemsModel.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace yave::app {

namespace {

/// 尺 (フレーム) を "12.3s" 形式にする。timebase は 59.94 前提の概算で足りる
/// (一覧の見出しであり、編集の判断に使う値ではない)。
QString durationText(qint64 frames)
{
    if (frames <= 0)
        return {};
    const double seconds = double(frames) / 60.0;
    return QStringLiteral("%1s").arg(seconds, 0, 'f', 1);
}

} // anonymous namespace

LibraryItemsModel::LibraryItemsModel(QObject* parent) : QAbstractListModel(parent)
{
    auto& store = LibraryStore::instance();
    connect(&store, &LibraryStore::itemsChanged, this, [this](int cat) {
        if (cat == int(category_))
            reload();
    });
    connect(&store, &LibraryStore::foldersChanged, this, [this](int cat) {
        if (cat == int(category_))
            reload();
    });
    connect(&store, &LibraryStore::iconChanged, this, [this](const QString&) { reload(); });
}

void LibraryItemsModel::setCurrentFolder(int category, const QString& folderId)
{
    if (category < 0 || category >= kLibraryCategoryCount)
        return;
    category_ = LibraryCategory(category);
    folderId_ = QUuid(folderId);
    reload();
}

void LibraryItemsModel::setFilterText(const QString& t)
{
    if (t == filterText_)
        return;
    filterText_ = t;
    emit filterTextChanged();
    reload();
}

void LibraryItemsModel::reload()
{
    beginResetModel();
    items_ = LibraryStore::instance().items(category_, folderId_);

    if (!filterText_.trimmed().isEmpty()) {
        const QString needle = filterText_.trimmed();
        std::vector<LibraryItem> filtered;
        for (auto& item : items_) {
            if (item.name.contains(needle, Qt::CaseInsensitive))
                filtered.push_back(item);
        }
        items_ = std::move(filtered);
    }
    endResetModel();
    emit countChanged();
}

int LibraryItemsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(items_.size());
}

QVariant LibraryItemsModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= int(items_.size()))
        return {};
    const LibraryItem& item = items_[size_t(index.row())];

    switch (role) {
    case Qt::DisplayRole:
    case NameRole:     return item.name;
    case ItemIdRole:   return item.itemId;
    case KindRole:     return item.kind;
    case CategoryRole: return int(item.category);
    case AssetIdRole:  return item.assetId.toString(QUuid::WithoutBraces);
    case DurationRole: return double(item.durationFrames);
    case MissingRole:  return item.missing;

    case IconSourceRole: {
        // 解決順序は 1.7.5:
        //   1. ユーザー指定 -> 2. サムネイル -> 3. 組み込み SVG (QML 側で解決)
        if (!item.iconOverride.isEmpty())
            return QUrl::fromLocalFile(item.iconOverride).toString();
        if (!item.assetId.isNull()
            && (item.kind == QLatin1String("video") || item.kind == QLatin1String("image")
                || item.kind == QLatin1String("generated"))) {
            return QStringLiteral("image://yave-thumb/%1")
                .arg(item.assetId.toString(QUuid::WithoutBraces));
        }
        return QString();   ///< QML が iconKey から組み込み SVG を選ぶ
    }

    case IconKeyRole:
        return QStringLiteral("kind_%1").arg(item.kind);

    case DetailTextRole: {
        if (item.missing)
            return QCoreApplication::translate("Library", "Missing");
        const QString dur = durationText(item.durationFrames);
        return dur.isEmpty() ? item.kind : dur;
    }

    case DragPayloadRole:
        return dragPayload(index.row());

    default:
        return {};
    }
}

QHash<int, QByteArray> LibraryItemsModel::roleNames() const
{
    return {
        { ItemIdRole,      "itemId" },
        { NameRole,        "name" },
        { KindRole,        "kind" },
        { CategoryRole,    "category" },
        { IconSourceRole,  "iconSource" },
        { IconKeyRole,     "iconKey" },
        { DurationRole,    "durationFrames" },
        { DetailTextRole,  "detailText" },
        { MissingRole,     "missing" },
        { DragPayloadRole, "dragPayload" },
        { AssetIdRole,     "assetId" },
    };
}

QString LibraryItemsModel::dragPayload(int row) const
{
    if (row < 0 || row >= int(items_.size()))
        return {};
    const LibraryItem& item = items_[size_t(row)];

    QJsonObject o;
    o[QStringLiteral("category")] = libraryCategoryKey(item.category);
    o[QStringLiteral("itemId")]   = item.itemId;
    o[QStringLiteral("assetId")]  = item.assetId.isNull()
                                        ? QString()
                                        : item.assetId.toString(QUuid::WithoutBraces);
    o[QStringLiteral("name")]     = item.name;
    o[QStringLiteral("duration")] = double(item.durationFrames);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QString LibraryItemsModel::itemIdAt(int row) const
{
    if (row < 0 || row >= int(items_.size()))
        return {};
    return items_[size_t(row)].itemId;
}

void LibraryItemsModel::setIcon(int row, const QUrl& fileUrl)
{
    if (row < 0 || row >= int(items_.size()) || !fileUrl.isLocalFile())
        return;
    LibraryStore::instance().setItemIcon(items_[size_t(row)].itemId, fileUrl.toLocalFile());
}

void LibraryItemsModel::clearIcon(int row)
{
    if (row < 0 || row >= int(items_.size()))
        return;
    LibraryStore::instance().clearItemIcon(items_[size_t(row)].itemId);
}

} // namespace yave::app
