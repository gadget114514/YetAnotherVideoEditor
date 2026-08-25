#pragma once

#include "Rational.h"

#include <QDir>
#include <QHash>
#include <QObject>
#include <QSize>
#include <QString>
#include <QUuid>

namespace yave {

/// 登録済み素材 1 個分の情報。
struct Asset
{
    enum class Kind { Video, Audio, Image, Generated };

    QUuid   id;
    Kind    kind = Kind::Video;

    /// プロジェクト相対パス ('/' 区切り)。絶対パスは持たない。
    QString relativePath;

    /// 解決できた場合の絶対パス。見つからなければ空。
    QString resolvedAbsolutePath;

    /// SHA-256 の先頭 16 バイト相当のハッシュ文字列
    QString hash;
    bool    isMissing = false;

    // --- プローブ結果 ---
    int64_t durationFrames = 0;
    Rational frameRate{1001, 60000};
    QSize   resolution;      ///< 動画 / 画像のみ
    bool    hasAudio = false;

    QUuid generatedByTaskId;   ///< AI 生成物の場合

    bool isValid() const { return !id.isNull(); }
};

/// 素材(ファイル)の登録・参照カウント・プロキシ管理。
class AssetLibrary : public QObject
{
    Q_OBJECT
public:
    explicit AssetLibrary(QObject* parent = nullptr);
    ~AssetLibrary() override;

    /// 素材を登録する。既存ならそのまま返す (id 安定)。
    Asset* registerAsset(const QString& absolutePath, Asset::Kind kind);

    /// 既知の id で登録する (プロジェクト読み込み用)。
    Asset* addResolvedAsset(const Asset& asset);

    void   removeAsset(const QUuid& id);
    Asset* asset(const QUuid& id) const;
    Asset* findByPath(const QString& normalizedRelativePath) const;

    QList<QUuid> allIds() const { return assets_.keys(); }
    int          count() const { return int(assets_.size()); }

    /// 参照カウント (クリップからの参照数)。UI の「未使用アセット」表示に使う。
    int  refCount(const QUuid& id) const { return refCounts_.value(id, 0); }
    void retain(const QUuid& id) { ++refCounts_[id]; }
    void release(const QUuid& id)
    {
        auto it = refCounts_.find(id);
        if (it != refCounts_.end() && --it.value() <= 0)
            refCounts_.erase(it);
    }

    /// プロキシ(低解像度)メディアのパス。未生成なら空。
    QString proxyPath(const QUuid& id) const { return proxies_.value(id); }
    void    setProxyPath(const QUuid& id, const QString& path) { proxies_.insert(id, path); }

signals:
    void assetAdded(const QUuid& id);
    void assetRemoved(const QUuid& id);

private:
    QHash<QUuid, Asset>     assets_;
    QHash<QUuid, int>       refCounts_;
    QHash<QUuid, QString>   proxies_;
};

} // namespace yave
