#include "SchemaMigration.h"

#include "JsonKeys.h"
#include "ProjectSerializer.h"     // LoadResult

#include <QJsonArray>

namespace yave::io {

using namespace keys;

// ===========================================================================
//  v1 -> v2: AIトラック (13章) の追加
//
//  追加のみで破壊的変更は無いが、それでもバージョンを上げる。
//  v1 のライタが v2 のプロジェクトを保存すると storyBible を丸ごと落とすため、
//  「未知フィールド保持が入っているかどうか」をバージョンで判別できる必要がある。
// ===========================================================================

void migrate_1_to_2(QJsonObject& root, LoadResult* result)
{
    Q_UNUSED(result);

    // (1) 作品設定を空で用意する
    if (!root.contains(QLatin1String("storyBible")))
        root[QLatin1String("storyBible")] = QJsonObject{};

    // (2) 各トラックへ AIトラック関連の既定値を入れる
    QJsonArray tracks = root[kTracks].toArray();
    for (int i = 0; i < tracks.size(); ++i) {
        QJsonObject t = tracks[i].toObject();
        if (!t.contains(QLatin1String("aiRole")))
            t[QLatin1String("aiRole")] = QString();
        if (!t.contains(QLatin1String("storyboardTrackId")))
            t[QLatin1String("storyboardTrackId")] = QJsonValue::Null;
        if (!t.contains(QLatin1String("roleDefaults")))
            t[QLatin1String("roleDefaults")] = QJsonObject{};
        tracks[i] = t;
    }
    root[kTracks] = tracks;

    // v1 のプロジェクトには CutClip も storyboard トラックも存在しないため、
    // クリップ側の変換は不要である。
}

// ===========================================================================
//  v2 -> v3: フィルタ / トランジション / ライブラリの追加
// ===========================================================================

void migrate_2_to_3(QJsonObject& root, LoadResult* result)
{
    Q_UNUSED(result);

    // (1) ライブラリのフォルダ構成を空で用意する
    if (!root.contains(kLibrary)) {
        root[kLibrary] = QJsonObject{
            { kFolders,     QJsonArray{} },
            { kAssignments, QJsonObject{} } };
    }

    QJsonArray tracks = root[kTracks].toArray();
    for (int i = 0; i < tracks.size(); ++i) {
        QJsonObject t = tracks[i].toObject();

        // (2) トランジション配列を空で用意する
        if (!t.contains(kTransitions))
            t[kTransitions] = QJsonArray{};

        // (3) クリップの effectChain を filters へ移す。
        //     トラックの effectChain (オーディオ) はそのまま残す。
        QJsonArray clips = t[kClips].toArray();
        for (int j = 0; j < clips.size(); ++j) {
            QJsonObject c = clips[j].toObject();
            if (c.contains(kEffectChain) && !c.contains(kFilters))
                c[kFilters] = c.take(kEffectChain);
            if (!c.contains(kFilters))
                c[kFilters] = QJsonArray{};
            clips[j] = c;
        }
        t[kClips] = clips;
        tracks[i] = t;
    }
    root[kTracks] = tracks;

    // v2 には title 型のクリップが存在しないため、クリップ型の変換は不要である。
}

} // namespace yave::io
