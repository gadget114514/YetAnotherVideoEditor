#pragma once

#include "SubtitleStyle.h"
#include "SubtitleText.h"

#include <yave/sdk/ISubtitleEffect.h>

#include <QHash>
#include <QSize>

namespace yave::subtitle {

/// QTextLayout を使ってテキストを行に折り返し、グリフ単位に分解する。
///
/// canvasSize は「出力解像度」であり、プレビュー解像度ではない。
/// 字幕は必ず出力解像度でラスタライズする (6.8 参照)。
class SubtitleLayout
{
public:
    static yave::sdk::SubtitleGlyphRun layout(const SubtitleText& text,
                                              const SubtitleStyle& style,
                                              const QSize& canvasSize);

    /// cacheKey の計算。hash(text, style, canvasSize)
    static quint64 cacheKey(const SubtitleText& text, const SubtitleStyle& style,
                            const QSize& canvasSize);
};

/// レイアウト結果のキャッシュ。
/// キーは (clipId, contentRevision)。テキスト / スタイル変更時のみ再計算される。
class SubtitleLayoutCache
{
public:
    const yave::sdk::SubtitleGlyphRun& get(const QUuid& clipId, uint64_t revision,
                                           const SubtitleText& text,
                                           const SubtitleStyle& style,
                                           const QSize& canvasSize);

    void clear() { entries_.clear(); }
    int  size() const { return int(entries_.size()); }

private:
    struct Entry
    {
        uint64_t                    revision = 0;
        yave::sdk::SubtitleGlyphRun run;
    };
    QHash<QUuid, Entry> entries_;
};

} // namespace yave::subtitle
