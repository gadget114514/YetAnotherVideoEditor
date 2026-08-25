#include "SubtitleText.h"

#include <QRegularExpression>

#include <algorithm>
#include <iterator>

namespace yave::subtitle {

void SubtitleText::addSpan(const TextSpan& s)
{
    if (s.length <= 0)
        return;
    spans_.push_back(s);
}

void SubtitleText::normalizeSpans()
{
    const int len = plain_.length();
    spans_.erase(std::remove_if(spans_.begin(), spans_.end(),
                                [len](const TextSpan& s) {
                                    return s.start >= len || s.start + s.length > len;
                                }),
                 spans_.end());
}

int SubtitleText::lineCount() const
{
    if (plain_.isEmpty())
        return 0;
    int n = 1;
    for (const QChar ch : plain_)
        if (ch == QChar::LineFeed)
            ++n;
    return n;
}

// ===========================================================================
//  SRT インラインマークアップ
// ===========================================================================

SubtitleText SubtitleText::fromSrtMarkup(const QString& markup)
{
    SubtitleText out;

    // <br> や実体参照を正規化する
    QString s = markup;
    s.replace(QLatin1String("<br/>"), QLatin1String("\n"), Qt::CaseInsensitive);
    s.replace(QLatin1String("<br />"), QLatin1String("\n"), Qt::CaseInsensitive);
    s.replace(QLatin1String("<br>"),   QLatin1String("\n"), Qt::CaseInsensitive);

    // タグを除去しつつ span を構築する。
    // 単純なスタックベースのパーサ (b/i/u/font のみ対応。SRT 規格上これで十分)。
    struct OpenTag
    {
        enum class Kind { Bold, Italic, Underline, Font } kind;
        int     start = 0;
        QColor  color;
        QString fontFamily;
    };
    std::vector<OpenTag> stack;

    static const QRegularExpression tagRe(
        QStringLiteral("</?\\s*(b|i|u|font)(\\s+[^>]*)?>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression colorAttrRe(
        QStringLiteral("color\\s*=\\s*[\"']?(#[0-9a-fA-F]{3,8}|[a-zA-Z]+)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression faceAttrRe(
        QStringLiteral("face\\s*=\\s*[\"']([^\"']*)"),
        QRegularExpression::CaseInsensitiveOption);

    qsizetype pos = 0;
    auto emitText = [&out](const QString& text) {
        if (!text.isEmpty())
            out.plain_ += text;
    };

    qsizetype searchFrom = 0;
    while (true) {
        const auto m = tagRe.match(s, searchFrom);
        if (!m.hasMatch()) {
            emitText(s.mid(pos));
            break;
        }

        emitText(s.mid(pos, m.capturedStart() - pos));

        const bool isClose = m.captured(0).startsWith(QStringLiteral("</"));
        const QString tagName = m.captured(1).toLower();

        if (!isClose) {
            OpenTag t;
            t.start = out.plain_.length();
            if (tagName == QLatin1String("b"))         t.kind = OpenTag::Kind::Bold;
            else if (tagName == QLatin1String("i"))    t.kind = OpenTag::Kind::Italic;
            else if (tagName == QLatin1String("u"))    t.kind = OpenTag::Kind::Underline;
            else {
                t.kind = OpenTag::Kind::Font;
                const auto cm = colorAttrRe.match(m.captured(2));
                if (cm.hasMatch())
                    t.color = QColor(cm.captured(1));
                const auto fm = faceAttrRe.match(m.captured(2));
                if (fm.hasMatch())
                    t.fontFamily = fm.captured(1);
            }
            stack.push_back(t);
        } else {
            // 対応する開始タグを探して閉じる
            const auto kindOf = [](const QString& name) {
                if (name == QLatin1String("b"))   return OpenTag::Kind::Bold;
                if (name == QLatin1String("i"))   return OpenTag::Kind::Italic;
                if (name == QLatin1String("u"))   return OpenTag::Kind::Underline;
                return OpenTag::Kind::Font;
            };
            const OpenTag::Kind k = kindOf(tagName);
            for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
                if (it->kind == k) {
                    TextSpan sp;
                    sp.start  = it->start;
                    sp.length = out.plain_.length() - it->start;
                    switch (it->kind) {
                    case OpenTag::Kind::Bold:      sp.bold = true; break;
                    case OpenTag::Kind::Italic:    sp.italic = true; break;
                    case OpenTag::Kind::Underline: sp.underline = true; break;
                    case OpenTag::Kind::Font:
                        if (it->color.isValid())   sp.color = it->color;
                        if (!it->fontFamily.isEmpty()) sp.fontFamily = it->fontFamily;
                        break;
                    }
                    if (sp.length > 0)
                        out.spans_.push_back(sp);
                    // 同一要素までのスタックを巻き戻す (未閉鎖タグの掃除)
                    stack.erase(std::next(it).base(), stack.end());
                    break;
                }
            }
        }

        pos = m.capturedEnd();
        searchFrom = pos;
    }

    return out;
}

QString SubtitleText::toSrtMarkup() const
{
    // スパンを開始位置順に処理し、SRT のタグへ戻す。
    // 入れ子の復元は完璧には行わない (SRT の慣習上、単純な前後配置で十分)。
    QString out;
    out.reserve(plain_.size());

    // 各文字位置での有効な書式を集める方式 (重なりをマージ)
    const int n = plain_.length();
    std::vector<bool> bold(n, false), italic(n, false), underline(n, false);
    std::vector<QString> colors(n), fonts(n);

    for (const TextSpan& s : spans_) {
        for (int i = s.start; i < s.start + s.length && i < n; ++i) {
            if (s.bold.value_or(false))       bold[i] = true;
            if (s.italic.value_or(false))     italic[i] = true;
            if (s.underline.value_or(false))  underline[i] = true;
            if (s.color)                      colors[i] = s.color->name(QColor::HexRgb);
            if (s.fontFamily)                 fonts[i] = *s.fontFamily;
        }
    }

    auto openTags = [&](int i) -> QString {
        QString t;
        const bool fontNow   = !fonts[i].isEmpty();
        const bool fontPrev  = (i > 0 && fonts[i - 1] == fonts[i]);
        const bool colorNow  = !colors[i].isEmpty();
        const bool colorPrev = (i > 0 && colors[i - 1] == colors[i]);
        if (fontNow && !fontPrev)
            t += QStringLiteral("<font face=\"%1\"%2>")
                     .arg(fonts[i],
                          colorNow ? QStringLiteral(" color=\"%1\"").arg(colors[i])
                                   : QString());
        else if (colorNow && !colorPrev)
            t += QStringLiteral("<font color=\"%1\">").arg(colors[i]);
        if (bold[i] && (i == 0 || !bold[i - 1]))      t += QStringLiteral("<b>");
        if (italic[i] && (i == 0 || !italic[i - 1]))  t += QStringLiteral("<i>");
        if (underline[i] && (i == 0 || !underline[i - 1])) t += QStringLiteral("<u>");
        return t;
    };
    auto closeTags = [&](int i) -> QString {
        QString t;
        if (underline[i] && (i + 1 >= n || !underline[i + 1])) t += QStringLiteral("</u>");
        if (italic[i] && (i + 1 >= n || !italic[i + 1]))       t += QStringLiteral("</i>");
        if (bold[i] && (i + 1 >= n || !bold[i + 1]))           t += QStringLiteral("</b>");
        const bool fontNext  = (i + 1 < n && fonts[i + 1] == fonts[i]);
        const bool colorNext = (i + 1 < n && colors[i + 1] == colors[i]);
        if (!fonts[i].isEmpty() && !fontNext)
            t += QStringLiteral("</font>");
        else if (!colors[i].isEmpty() && !colorNext)
            t += QStringLiteral("</font>");
        return t;
    };

    for (int i = 0; i < n; ++i) {
        out += openTags(i);
        out += plain_.at(i);
        out += closeTags(i);
    }
    return out;
}

} // namespace yave::subtitle
