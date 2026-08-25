#pragma once

#include "../library/LibraryStore.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>
#include <QUuid>

#include <vector>

namespace yave::app {

/// 選択中フォルダの中身 (1.7.5)。
/// 一覧 / 小アイコン / 大アイコン / 詳細 の 4 モードすべてがこの 1 モデルを見る。
class LibraryItemsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles
    {
        ItemIdRole = Qt::UserRole + 1,
        NameRole,
        KindRole,          ///< "video" / "audio" / "transition" ...
        CategoryRole,
        IconSourceRole,    ///< 解決済み URL (上書き -> サムネ -> 組み込み)
        IconKeyRole,       ///< 組み込みアイコンのキー
        DurationRole,      ///< フレーム。0 なら尺なし
        DetailTextRole,    ///< 詳細ビューの 2 列目
        MissingRole,
        DragPayloadRole,   ///< D&D 用 JSON
        AssetIdRole,
    };

    explicit LibraryItemsModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const { return filterText_; }
    void    setFilterText(const QString& t);

    /// ツリーの選択に追従させる。
    Q_INVOKABLE void setCurrentFolder(int category, const QString& folderId);

    Q_INVOKABLE QString dragPayload(int row) const;
    Q_INVOKABLE QString itemIdAt(int row) const;

    /// 右クリックメニューから呼ぶ (1.7.5 のアイコン差し替え)。
    Q_INVOKABLE void setIcon(int row, const QUrl& fileUrl);
    Q_INVOKABLE void clearIcon(int row);

signals:
    void filterTextChanged();
    void countChanged();

private:
    void reload();

    LibraryCategory          category_ = LibraryCategory::Media;
    QUuid                    folderId_;
    QString                  filterText_;
    std::vector<LibraryItem> items_;
};

} // namespace yave::app
