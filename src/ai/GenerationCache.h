#pragma once

#include "AiGenerationParams.h"

#include <QDateTime>
#include <QDir>
#include <QHash>
#include <QString>

namespace yave::ai {

/// 生成物のキャッシュ。
///
/// キーは AiGenerationParams::contentHash() (配置先を含まない正規化ハッシュ)。
/// 同一パラメータでの再生成はキャッシュヒットさせることで、
/// 「試して、気に入らなければ戻す」操作を高速にする。
class GenerationCache
{
public:
    explicit GenerationCache(const QString& rootDir);

    /// 同一パラメータの既存成果物を探す。無ければ空文字列。
    QString lookup(const QByteArray& contentHash) const;

    /// 成果物をキャッシュへ登録する。戻り値は保存先パス。
    QString store(const QByteArray& contentHash, const QString& filePath);

    void remove(const QByteArray& contentHash);

    qint64 totalSizeBytes() const { return totalBytes_; }

private:
    struct Entry
    {
        QString   path;
        QDateTime createdAt;
    };

    QDir   root_;
    QHash<QByteArray, Entry> entries_;
    qint64 totalBytes_ = 0;

    void loadIndex();
    void saveIndex();
};

} // namespace yave::ai
