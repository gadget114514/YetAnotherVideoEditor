#include "PathResolver.h"

#include <QDir>
#include <QFileInfo>

namespace yave::io {

PathResolver::PathResolver(const QString& projectFilePath)
{
    const QFileInfo info(projectFilePath);
    projectDir_ = info.exists() ? info.dir() : QDir(info.absolutePath());
}

QString PathResolver::normalizeSeparators(const QString& p)
{
    // '\' -> '/'. 読み込み時に toNativeSeparators は使わない。
    // Qt のファイル API は '/' をどのプラットフォームでも受け付ける。
    return QDir::fromNativeSeparators(p);
}

QString PathResolver::toRelative(const QString& absolutePath, bool* relativizedOut) const
{
    if (relativizedOut)
        *relativizedOut = false;

    const QString norm = normalizeSeparators(QFileInfo(absolutePath).absoluteFilePath());
    QString rel = projectDir_.relativeFilePath(norm);

    if (rel.isEmpty())
        rel = norm;
    // ../ で始まる (別ドライブ / プロジェクト外) 場合は絶対パスのまま返し、
    // 呼び出し側に「収集」を提案させる (3.4 参照)。
    if (rel.startsWith(QLatin1String("../"))) {
        return norm;
    }
    if (relativizedOut)
        *relativizedOut = true;
    return QDir::cleanPath(rel);
}

QString PathResolver::toAbsolute(const QString& relativePath, bool* resolvedOut) const
{
    if (resolvedOut)
        *resolvedOut = false;

    const QString norm = normalizeSeparators(relativePath);
    if (QDir::isAbsolutePath(norm))
        return norm;

    // 1. プロジェクトフォルダからの相対
    const QString candidate = projectDir_.filePath(norm);
    if (QFileInfo::exists(candidate)) {
        if (resolvedOut)
            *resolvedOut = true;
        return QDir::cleanPath(candidate);
    }

    // 2. assets/ 直下 (ファイル名一致)
    const QString fileName = QFileInfo(norm).fileName();
    const QDir assetsDir = projectDir_.filePath(QStringLiteral("assets"));
    const QFileInfoList entries =
        assetsDir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& fi : entries)
        if (fi.fileName() == fileName)
            return QDir::cleanPath(fi.absoluteFilePath());

    // 見つからない。missing フラグは呼び出し側 (AssetLibrary) が立てる。
    return candidate;
}

} // namespace yave::io
