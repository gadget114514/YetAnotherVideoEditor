#pragma once

#include <QHash>
#include <QString>
#include <QUuid>

#include <vector>

namespace yave {

/// メディアライブラリのフォルダ 1 個 (1.7.5 / 9.2.1)。
///
/// エフェクトライブラリ側のフォルダはアプリ設定 (QSettings) に置くのに対し、
/// こちらは**プロジェクトの内容**なので Project が持ち、.yave へ保存される。
struct MediaFolder
{
    QUuid   id;
    QUuid   parentId;        ///< null ならルート直下
    QString name;
};

/// フォルダ木 + 「どのアセットがどのフォルダに居るか」。
struct MediaFolderTree
{
    std::vector<MediaFolder> folders;
    QHash<QUuid, QUuid>      assignments;   ///< assetId -> folderId

    const MediaFolder* find(const QUuid& folderId) const
    {
        for (const auto& f : folders) {
            if (f.id == folderId)
                return &f;
        }
        return nullptr;
    }

    /// 存在しないフォルダを指す割り当てを捨てる (9.2.1)。
    /// 壊れた参照でアセットが一覧から消えるほうが実害が大きいため、
    /// 読み込み時に黙ってルート直下へ落とす。
    void dropDanglingAssignments()
    {
        for (auto it = assignments.begin(); it != assignments.end(); ) {
            if (!it.value().isNull() && !find(it.value()))
                it = assignments.erase(it);
            else
                ++it;
        }
    }
};

} // namespace yave
