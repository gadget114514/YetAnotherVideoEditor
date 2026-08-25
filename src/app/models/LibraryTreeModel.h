#pragma once

#include "../library/LibraryStore.h"

#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QStringList>
#include <QUuid>

#include <vector>

namespace yave::app {

/// ライブラリのフォルダツリー (1.7.5)。
///
/// ルートはこのパネルが担当するカテゴリ、子はフォルダのみ。アイテムは
/// LibraryItemsModel が別に持つ (エクスプローラの左ペイン / 右ペインと同じ分担)。
///
/// データ量が小さい (フォルダ数は多くて数十) ため、変更時はノードキャッシュを
/// 作り直して reset する。差分通知を書くより壊れにくい。
class LibraryTreeModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QStringList categories READ categories WRITE setCategories NOTIFY categoriesChanged)
public:
    enum Roles
    {
        NameRole = Qt::UserRole + 1,
        FolderIdRole,        ///< QString。カテゴリルートでは空
        CategoryRole,        ///< int (LibraryCategory)
        CategoryKeyRole,     ///< QString "media" 等
        IsCategoryRootRole,
        CanRenameRole,
        IconKeyRole,         ///< "cat_media" / "folder"
    };

    explicit LibraryTreeModel(QObject* parent = nullptr);

    QStringList categories() const { return categoryKeys_; }
    void setCategories(const QStringList& keys);

    // --- QAbstractItemModel ---
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int         rowCount(const QModelIndex& parent = {}) const override;
    int         columnCount(const QModelIndex& parent = {}) const override;
    QVariant    data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // --- QML から呼ぶ操作 (1.7.5) ---
    Q_INVOKABLE QModelIndex createFolder(const QModelIndex& parent, const QString& name);
    Q_INVOKABLE bool        renameFolder(const QModelIndex& index, const QString& name);
    Q_INVOKABLE bool        removeFolder(const QModelIndex& index);
    Q_INVOKABLE bool        dropItems(const QModelIndex& folderIndex, const QStringList& itemIds);
    Q_INVOKABLE bool        dropFolder(const QModelIndex& folderIndex, const QString& folderId);

    /// QML の TreeView から選択を伝えるためのヘルパ。
    Q_INVOKABLE int     categoryAt(const QModelIndex& index) const;
    Q_INVOKABLE QString folderIdAt(const QModelIndex& index) const;

signals:
    void categoriesChanged();

private:
    struct Node
    {
        QUuid           folderId;      ///< null ならカテゴリルート
        LibraryCategory category = LibraryCategory::Media;
        QString         name;
        int             parent = -1;
        std::vector<int> children;
    };

    void rebuild();
    void appendFolders(int parentNode, LibraryCategory cat, const QUuid& parentFolderId);
    const Node* nodeAt(const QModelIndex& index) const;

    QStringList       categoryKeys_;
    std::vector<Node> nodes_;          ///< 0 番以降がカテゴリルート
    std::vector<int>  roots_;
};

} // namespace yave::app
