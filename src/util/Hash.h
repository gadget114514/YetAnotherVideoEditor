#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QtGlobal>

#include <cstdint>

namespace yave {

/// FNV-1a 64bit。QString / QByteArray のキャッシュキー計算に使う。
inline quint64 fnv1a64(const char* data, size_t len, quint64 seed = 0xcbf29ce484222325ULL)
{
    quint64 h = seed;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<quint8>(data[i]);
        h *= 0x100000001b3ULL;
    }
    return h;
}

inline quint64 hashString(const QString& s)
{
    const QByteArray utf8 = s.toUtf8();
    return fnv1a64(utf8.constData(), size_t(utf8.size()));
}

inline quint64 hashBytes(const QByteArray& b) { return fnv1a64(b.constData(), size_t(b.size())); }

/// 複数ハッシュの結合。boost::hash_combine 相当。
inline void hashCombine(quint64& seed, quint64 v)
{
    seed ^= v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

} // namespace yave
