#include "LibraryTreeModel.h"

#include <QCoreApplication>

namespace yave::app {

LibraryTreeModel::LibraryTreeModel(QObject* parent) : QAbstractItemModel(parent)
{
    connect(&LibraryStore::instance(), &LibraryStore::foldersChanged,
            this, [this](int) { rebuild(); });
    rebuild();
}

void LibraryTreeModel::setCategories(const QStringList& keys)
{
    if (keys == categoryKeys_)
        return;
    categoryKeys_ = keys;
    rebuild();
    emit categoriesChanged();
}

// ===========================================================================
//  ノードキャッシュ
// ===========================================================================

void LibraryTreeModel::rebuild()
{
    beginResetModel();
    nodes_.clear();
    roots_.clear();

    for (const QString& key : categoryKeys_) {
        LibraryCategory cat = LibraryCategory::Media;
        if (!libraryCategoryFromKey(key, &cat))
            continue;

        Node root;
        root.category = cat;
        root.name     = libraryCategoryDisplayName(cat);
        root.parent   = -1;
        nodes_.push_back(std::move(root));
        const int rootIndex = int(nodes_.size()) - 1;
        roots_.push_back(rootIndex);

        appendFolders(rootIndex, cat, QUuid());
    }
    endResetModel();
}

void LibraryTreeModel::appendFolders(int parentNode, LibraryCategory cat,
                                     const QUuid& parentFolderId)
{
    for (const LibraryFolder& f : LibraryStore::instance().folders(cat, parentFolderId)) {
        Node n;
        n.folderId = f.id;
        n.category = cat;
        n.name     = f.name;
        n.parent   = parentNode;
        nodes_.push_back(std::move(n));

        const int idx = int(nodes_.size()) - 1;
        nodes_[size_t(parentNode)].children.push_back(idx);

        appendFolders(idx, cat, f.id);
    }
}

const LibraryTreeModel::Node* LibraryTreeModel::nodeAt(const QModelIndex& index) const
{
    if (!index.isValid())
        return nullptr;
    const int i = int(index.internalId());
    if (i < 0 || i >= int(nodes_.size()))
        return nullptr;
    return &nodes_[size_t(i)];
}

// ===========================================================================
//  QAbstractItemModel
// ===========================================================================

QModelIndex LibraryTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0 || row < 0)
        return {};

    if (!parent.isValid()) {
        if (row >= int(roots_.size()))
            return {};
        return createIndex(row, column, quintptr(roots_[size_t(row)]));
    }

    const Node* p = nodeAt(parent);
    if (!p || row >= int(p->children.size()))
        return {};
    return createIndex(row, column, quintptr(p->children[size_t(row)]));
}

QModelIndex LibraryTreeModel::parent(const QModelIndex& child) const
{
    const Node* c = nodeAt(child);
    if (!c || c->parent < 0)
        return {};

    const Node& p = nodes_[size_t(c->parent)];
    // 親自身の row を求める
    if (p.parent < 0) {
        for (int r = 0; r < int(roots_.size()); ++r) {
            if (roots_[size_t(r)] == c->parent)
                return createIndex(r, 0, quintptr(c->parent));
        }
        return {};
    }
    const Node& gp = nodes_[size_t(p.parent)];
    for (int r = 0; r < int(gp.children.size()); ++r) {
        if (gp.children[size_t(r)] == c->parent)
            return createIndex(r, 0, quintptr(c->parent));
    }
    return {};
}

int LibraryTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return int(roots_.size());
    const Node* p = nodeAt(parent);
    return p ? int(p->children.size()) : 0;
}

int LibraryTreeModel::columnCount(const QModelIndex&) const
{
    return 1;
}

QVariant LibraryTreeModel::data(const QModelIndex& index, int role) const
{
    const Node* n = nodeAt(index);
    if (!n)
        return {};

    const bool isRoot = n->folderId.isNull();
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:           return n->name;
    case FolderIdRole:       return isRoot ? QString() : n->folderId.toString(QUuid::WithoutBraces);
    case CategoryRole:       return int(n->category);
    case CategoryKeyRole:    return libraryCategoryKey(n->category);
    case IsCategoryRootRole: return isRoot;
    case CanRenameRole:      return !isRoot;      ///< カテゴリルートは改名できない (1.7.5)
    case IconKeyRole:
        return isRoot ? QStringLiteral("cat_%1").arg(libraryCategoryKey(n->category))
                      : QStringLiteral("folder");
    default:
        return {};
    }
}

QHash<int, QByteArray> LibraryTreeModel::roleNames() const
{
    return {
        { NameRole,           "name" },
        { FolderIdRole,       "folderId" },
        { CategoryRole,       "category" },
        { CategoryKeyRole,    "categoryKey" },
        { IsCategoryRootRole, "isCategoryRoot" },
        { CanRenameRole,      "canRename" },
        { IconKeyRole,        "iconKey" },
    };
}

// ===========================================================================
//  操作
// ===========================================================================

QModelIndex LibraryTreeModel::createFolder(const QModelIndex& parent, const QString& name)
{
    const Node* p = nodeAt(parent);
    if (!p)
        return {};

    const QString wanted = name.trimmed().isEmpty()
                               ? QCoreApplication::translate("Library", "New Folder")
                               : name.trimmed();
    const QUuid created = LibraryStore::instance().createFolder(p->category, p->folderId, wanted);
    if (created.isNull())
        return {};

    // rebuild() 後に新しいフォルダの index を探して返す (QML が改名モードへ入る)
    for (int i = 0; i < int(nodes_.size()); ++i) {
        if (nodes_[size_t(i)].folderId != created)
            continue;
        const Node& n = nodes_[size_t(i)];
        if (n.parent < 0)
            break;
        const Node& parentNode = nodes_[size_t(n.parent)];
        for (int r = 0; r < int(parentNode.children.size()); ++r) {
            if (parentNode.children[size_t(r)] == i)
                return createIndex(r, 0, quintptr(i));
        }
    }
    return {};
}

bool LibraryTreeModel::renameFolder(const QModelIndex& index, const QString& name)
{
    const Node* n = nodeAt(index);
    if (!n || n->folderId.isNull())
        return false;
    return LibraryStore::instance().renameFolder(n->folderId, name);
}

bool LibraryTreeModel::removeFolder(const QModelIndex& index)
{
    const Node* n = nodeAt(index);
    if (!n || n->folderId.isNull())
        return false;
    return LibraryStore::instance().removeFolder(n->folderId);
}

bool LibraryTreeModel::dropItems(const QModelIndex& folderIndex, const QStringList& itemIds)
{
    const Node* n = nodeAt(folderIndex);
    if (!n)
        return false;
    return LibraryStore::instance().moveItems(itemIds, n->category, n->folderId);
}

bool LibraryTreeModel::dropFolder(const QModelIndex& folderIndex, const QString& folderId)
{
    const Node* n = nodeAt(folderIndex);
    if (!n)
        return false;
    return LibraryStore::instance().moveFolder(QUuid(folderId), n->folderId);
}

int LibraryTreeModel::categoryAt(const QModelIndex& index) const
{
    const Node* n = nodeAt(index);
    return n ? int(n->category) : -1;
}

QString LibraryTreeModel::folderIdAt(const QModelIndex& index) const
{
    const Node* n = nodeAt(index);
    if (!n || n->folderId.isNull())
        return {};
    return n->folderId.toString(QUuid::WithoutBraces);
}

} // namespace yave::app
