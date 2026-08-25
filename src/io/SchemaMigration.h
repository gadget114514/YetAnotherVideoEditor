#pragma once

#include <QJsonObject>
#include <QStringList>
#include <vector>

namespace yave::io {

struct LoadResult;

using MigrationFn = void (*)(QJsonObject& root, LoadResult* result);

/// バージョン N から N+1 へ変換する関数のテーブル
struct Migration
{
    int fromVersion;
    MigrationFn fn;
};

/// v1 -> v2: AIトラック (13章) の追加。追加のみで破壊的変更は無い。
void migrate_1_to_2(QJsonObject& root, LoadResult* result);

/// v2 -> v3: フィルタ / トランジション / ライブラリの追加 (9.11)。
/// クリップの effectChain を filters へ改称する破壊的変更を含む。
void migrate_2_to_3(QJsonObject& root, LoadResult* result);

inline const std::vector<Migration>& migrations()
{
    static const std::vector<Migration> table = {
        { 1, &migrate_1_to_2 },
        { 2, &migrate_2_to_3 },
    };
    return table;
}

} // namespace yave::io
