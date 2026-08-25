#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QUuid>

#include <vector>

namespace yave {
class Project;
}

namespace yave::app {

/// ライブラリのカテゴリ (1.7.5)。
/// Media だけがプロジェクト所有、残りはアプリ所有のカタログ。
enum class LibraryCategory
{
    Media = 0,
    Transition,
    Title,
    Subtitle,
    Filter,
    Effect,
};
constexpr int kLibraryCategoryCount = 6;

QString libraryCategoryKey(LibraryCategory c);            ///< "media" / "transition" ...
bool    libraryCategoryFromKey(const QString& key, LibraryCategory* out);
QString libraryCategoryDisplayName(LibraryCategory c);    ///< 翻訳済みの表示名

struct LibraryFolder
{
    QUuid           id;
    QUuid           parentId;      ///< null ならカテゴリ直下
    QString         name;
    LibraryCategory category = LibraryCategory::Media;
};

struct LibraryItem
{
    QString         itemId;        ///< メディアはアセット UUID 文字列、他は "yave.trans.dissolve" 等
    LibraryCategory category = LibraryCategory::Media;
    QString         name;          ///< 表示名 (翻訳済み)
    QUuid           folderId;      ///< null ならカテゴリ直下
    QUuid           assetId;       ///< Media のみ
    QString         kind;          ///< "video"/"audio"/"image"/"transition"/... アイコン選択に使う
    QString         iconOverride;  ///< ユーザーが割り当てたアイコンの絶対パス
    bool            builtin = false;
    qint64          durationFrames = 0;   ///< Media のみ
    bool            missing = false;      ///< Media: ファイルが見つからない
};

/// 6 カテゴリのフォルダ木とアイテムを一元管理する (1.7.5)。
///
/// 供給元が異なるもの (AssetLibrary / 組み込みテーブル / PluginManager) を
/// 1 つの形にそろえるのがこのクラスの役目。永続化先はカテゴリで分かれる:
///   Media           -> プロジェクト JSON の library セクション (9.2.1)
///   それ以外の 5 種  -> QSettings ui/library/*
class LibraryStore : public QObject
{
    Q_OBJECT
public:
    static LibraryStore& instance();

    /// Media カテゴリの供給元を差し替える。プロジェクトを開くたびに呼ぶ。
    void setProject(Project* project);
    Project* project() const { return project_; }

    // ---- 参照 ----
    std::vector<LibraryFolder> folders(LibraryCategory cat, const QUuid& parentId) const;
    std::vector<LibraryItem>   items(LibraryCategory cat, const QUuid& folderId) const;
    bool                       hasSubfolders(LibraryCategory cat, const QUuid& folderId) const;
    const LibraryFolder*       folder(const QUuid& folderId) const;

    // ---- フォルダ操作 ----
    QUuid createFolder(LibraryCategory cat, const QUuid& parentId, const QString& name);
    bool  renameFolder(const QUuid& folderId, const QString& newName);
    /// 中のアイテムとサブフォルダは親へ繰り上げる。アイテム自体は消さない (1.7.5)。
    bool  removeFolder(const QUuid& folderId);
    bool  moveItems(const QStringList& itemIds, LibraryCategory cat, const QUuid& toFolderId);
    bool  moveFolder(const QUuid& folderId, const QUuid& toParentId);

    // ---- アイコン ----
    void    setItemIcon(const QString& itemId, const QString& absolutePath);
    void    clearItemIcon(const QString& itemId);
    QString itemIconOverride(const QString& itemId) const;

    /// メディアアイテムを取り込み直後に選択フォルダへ入れる。
    void assignAssetToFolder(const QUuid& assetId, const QUuid& folderId);

    /// AssetLibrary が変わったことを一覧へ伝える (取り込み / 削除の後に呼ぶ)。
    void refreshMedia() { emit itemsChanged(int(LibraryCategory::Media)); }

signals:
    void foldersChanged(int category);
    void itemsChanged(int category);
    void iconChanged(const QString& itemId);

private:
    LibraryStore();

    void rebuildCatalog();       ///< 組み込み + プラグイン由来のアイテムを作り直す
    void loadSettings();
    void saveSettings() const;

    Project* project_ = nullptr;

    /// カタログ側 (Media 以外) のフォルダ。Media のフォルダは Project が持つ。
    std::vector<LibraryFolder> catalogFolders_;

    /// カタログ側のアイテム。Media のアイテムは AssetLibrary から都度作る。
    std::vector<LibraryItem>   catalogItems_;

    /// itemId -> folderId (カタログ側のみ)
    QHash<QString, QUuid>      catalogAssignments_;
    QHash<QString, QString>    iconOverrides_;
};

} // namespace yave::app
