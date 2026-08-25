#include "GenerationCache.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace yave::ai {

GenerationCache::GenerationCache(const QString& rootDir)
    : root_(rootDir)
{
    QDir().mkpath(root_.absolutePath());
    loadIndex();
}

void GenerationCache::loadIndex()
{
    QFile f(root_.filePath(QStringLiteral("index.json")));
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();

    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        Entry e;
        e.path      = o[QStringLiteral("path")].toString();
        e.createdAt = QDateTime::fromString(o[QStringLiteral("at")].toString(), Qt::ISODate);
        if (!e.path.isEmpty()) {
            entries_.insert(o[QStringLiteral("hash")].toString().toUtf8(), e);
            totalBytes_ += QFileInfo(e.path).size();
        }
    }
}

void GenerationCache::saveIndex()
{
    QJsonArray arr;
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
        QJsonObject o;
        o[QStringLiteral("hash")] = QString::fromUtf8(it.key());
        o[QStringLiteral("path")] = it->path;
        o[QStringLiteral("at")]   = it->createdAt.toString(Qt::ISODate);
        arr.append(o);
    }
    QSaveFile f(root_.filePath(QStringLiteral("index.json")));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact)), f.commit();
}

QString GenerationCache::lookup(const QByteArray& contentHash) const
{
    auto it = entries_.constFind(contentHash);
    if (it == entries_.cend())
        return {};
    if (!QFileInfo::exists(it->path)) {
        return {};
    }
    return it->path;
}

QString GenerationCache::store(const QByteArray& contentHash, const QString& filePath)
{
    Entry e;
    e.path      = filePath;
    e.createdAt = QDateTime::currentDateTimeUtc();
    totalBytes_ += QFileInfo(filePath).size();
    entries_.insert(contentHash, std::move(e));
    saveIndex();
    return filePath;
}

void GenerationCache::remove(const QByteArray& contentHash)
{
    auto it = entries_.find(contentHash);
    if (it != entries_.end()) {
        QFile::remove(it->path);
        totalBytes_ -= QFileInfo(it->path).size();
        entries_.erase(it);
        saveIndex();
    }
}

} // namespace yave::ai
