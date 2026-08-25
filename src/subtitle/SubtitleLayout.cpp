#include "SubtitleLayout.h"
#include "SubtitleStylePreset.h"

#include "../util/Hash.h"

#include <QFont>
#include <QFontMetricsF>
#include <QTextLayout>
#include <QTextOption>

namespace yave::subtitle {

using yave::sdk::GlyphInfo;
using yave::sdk::SubtitleGlyphRun;

quint64 SubtitleLayout::cacheKey(const SubtitleText& text, const SubtitleStyle& style,
                                 const QSize& canvasSize)
{
    quint64 h = hashString(text.plain());
    hashCombine(h, hashString(style.fontFamily));
    hashCombine(h, quint64(style.fontPointSize * 100.0));
    hashCombine(h, quint64(style.fontWeight));
    hashCombine(h, style.fillColor.rgba());
    hashCombine(h, quint64(canvasSize.width()) * 0x100000000ULL + quint64(canvasSize.height()));
    return h;
}

SubtitleGlyphRun SubtitleLayout::layout(const SubtitleText& text,
                                        const SubtitleStyle& style,
                                        const QSize& canvasSize)
{
    SubtitleGlyphRun run;

    QFont font(style.fontFamily);
    font.setPointSizeF(style.fontPointSize);
    font.setWeight(QFont::Weight(style.fontWeight));
    font.setItalic(style.italic);
    font.setLetterSpacing(QFont::AbsoluteSpacing, qreal(style.letterSpacing));

    QTextLayout layout(text.plain(), font);
    layout.setCacheEnabled(true);

    // リッチスパンを QTextLayout::FormatRange へ変換
    QList<QTextLayout::FormatRange> formats;
    for (const TextSpan& s : text.spans()) {
        QTextCharFormat fmt;
        if (s.bold.has_value())
            fmt.setFontWeight(*s.bold ? QFont::Bold : QFont::Normal);
        if (s.italic.has_value())
            fmt.setFontItalic(*s.italic);
        if (s.underline.has_value())
            fmt.setFontUnderline(*s.underline);
        if (s.color)
            fmt.setForeground(QBrush(*s.color));
        if (s.fontFamily)
            fmt.setFontFamilies({*s.fontFamily});

        QTextLayout::FormatRange fr;
        fr.start  = s.start;
        fr.length = s.length;
        fr.format = fmt;
        formats.append(fr);
    }
    layout.setFormats(formats);

    QTextOption opt;
    opt.setWrapMode(QTextOption::WordWrap);
    switch (style.hAlign) {
    case HAlign::Left:   opt.setAlignment(Qt::AlignLeft);   break;
    case HAlign::Center: opt.setAlignment(Qt::AlignHCenter); break;
    case HAlign::Right:  opt.setAlignment(Qt::AlignRight);  break;
    }
    layout.setTextOption(opt);

    const qreal maxWidth = qreal(canvasSize.width()) * style.maxWidthRatio;
    const qreal leading  = QFontMetricsF(font).height() * (style.lineSpacing - 1.0);

    layout.beginLayout();
    qreal y = 0;
    int lineIdx = 0;
    std::vector<std::pair<QTextLine, int>> lines;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(maxWidth);
        line.setPosition(QPointF(0, y));
        y += line.height() + leading;
        lines.emplace_back(line, lineIdx++);
    }
    layout.endLayout();

    // 各行を文字単位に分解
    run.lineCount = lineIdx;
    run.blockSize = QSizeF(layout.boundingRect().width(), y);
    run.cacheKey  = cacheKey(text, style, canvasSize);

    int wordIdx = 0;
    const QString plain = text.plain();
    for (auto& [line, li] : lines) {
        for (int ci = line.textStart(); ci < line.textStart() + line.textLength(); ++ci) {
            const QChar ch = ci < plain.length() ? plain.at(ci) : QChar(u' ');
            GlyphInfo g;
            g.charIndex    = ci;
            g.lineIndex    = li;
            g.isWhitespace = ch.isSpace();
            if (g.isWhitespace)
                ++wordIdx;
            g.wordIndex = wordIdx;

            const qreal x0 = line.cursorToX(ci);
            const qreal x1 = line.cursorToX(ci + 1);
            g.layoutRect = QRectF(x0, line.y(), x1 - x0, line.height());

            // スパンから色を解決する
            QColor color = style.fillColor;
            for (const TextSpan& s : text.spans()) {
                if (ci >= s.start && ci < s.start + s.length && s.color) {
                    color = *s.color;
                    break;
                }
            }
            g.baseColor = color;
            run.glyphs.push_back(g);
        }
    }

    // アトラスサイズはラスタライザが確定させる。ここではブロック+パディングの目安を置く。
    constexpr int kPad = 8;
    run.atlasSize = QSize(int(std::ceil(run.blockSize.width())) + kPad * 2,
                          int(std::ceil(run.blockSize.height())) + kPad * 2);
    return run;
}

// ===========================================================================
//  キャッシュ
// ===========================================================================

const SubtitleGlyphRun& SubtitleLayoutCache::get(const QUuid& clipId, uint64_t revision,
                                                 const SubtitleText& text,
                                                 const SubtitleStyle& style,
                                                 const QSize& canvasSize)
{
    auto it = entries_.find(clipId);
    if (it != entries_.end() && it->revision == revision
        && it->run.cacheKey == SubtitleLayout::cacheKey(text, style, canvasSize))
        return it->run;

    Entry e;
    e.revision = revision;
    e.run      = SubtitleLayout::layout(text, style, canvasSize);
    it = entries_.insert(clipId, std::move(e));
    return it.value().run;
}

} // namespace yave::subtitle
