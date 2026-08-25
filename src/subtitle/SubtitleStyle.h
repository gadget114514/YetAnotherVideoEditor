#pragma once

#include <QColor>
#include <QMarginsF>
#include <QPointF>
#include <QString>

#include <optional>

namespace yave::subtitle {

enum class HAlign { Left, Center, Right };
enum class VAlign { Top, Middle, Bottom };

struct SubtitleStyle
{
    // --- フォント ---
    QString fontFamily    = QStringLiteral("Noto Sans JP");
    double  fontPointSize = 48.0;          ///< 出力解像度基準の pt
    int     fontWeight    = 700;           ///< QFont::Bold
    bool    italic        = false;

    // --- 色 ---
    QColor  fillColor    = Qt::white;
    QColor  outlineColor = Qt::black;
    double  outlineWidth = 3.0;            ///< px (出力解像度基準)
    QColor  shadowColor  = QColor(0, 0, 0, 160);
    QPointF shadowOffset = QPointF(2, 2);
    double  shadowBlur   = 4.0;

    // --- 背景ボックス ---
    bool      boxEnabled = false;
    QColor    boxColor   = QColor(0, 0, 0, 128);
    QMarginsF boxPadding = QMarginsF(12, 6, 12, 6);
    double    boxRadius  = 4.0;

    // --- レイアウト ---
    HAlign  hAlign        = HAlign::Center;
    VAlign  vAlign        = VAlign::Bottom;
    QPointF anchor        = QPointF(0.5, 0.92);   ///< 画面内正規化座標 (0..1)
    double  lineSpacing   = 1.2;                  ///< 行送り倍率
    double  letterSpacing = 0.0;                  ///< px
    double  maxWidthRatio = 0.86;                 ///< 画面幅に対する折り返し幅

    // --- 変換 ---
    double  rotationDeg = 0.0;
    QPointF scale       = QPointF(1.0, 1.0);
    double  opacity     = 1.0;

    // --- 縦書き ---
    bool vertical = false;                ///< 日本語縦書き

    bool operator==(const SubtitleStyle& o) const
    {
        return fontFamily == o.fontFamily && fontPointSize == o.fontPointSize
            && fontWeight == o.fontWeight && italic == o.italic && fillColor == o.fillColor;
    }
};

/// プリセットからの差分。設定されたフィールドのみ保持する。
///
/// プリセット + 差分方式の理由: SRT を 500 キュー取り込んだ後に
/// 「全部のフォントを変えたい」は必ず発生する要求。プリセット参照にしておけば
/// プリセットを 1 箇所変えるだけで済む。個別に変えたクリップだけが差分を持つ。
struct SubtitleStyleDiff
{
    std::optional<QString> fontFamily;
    std::optional<double>  fontPointSize;
    std::optional<int>     fontWeight;
    std::optional<bool>    italic;
    std::optional<QColor>  fillColor;
    std::optional<QColor>  outlineColor;
    std::optional<double>  outlineWidth;
    std::optional<QColor>  shadowColor;
    std::optional<double>  shadowBlur;
    std::optional<bool>    boxEnabled;
    std::optional<QColor>  boxColor;
    std::optional<int>     hAlign;          ///< 0=Left 1=Center 2=Right (JSON 用に数値)
    std::optional<int>     vAlign;          ///< 0=Top 1=Middle 2=Bottom
    std::optional<QPointF> anchor;
    std::optional<double>  lineSpacing;
    std::optional<double>  letterSpacing;
    std::optional<double>  maxWidthRatio;
    std::optional<double>  rotationDeg;
    std::optional<QPointF> scale;
    std::optional<double>  opacity;
    std::optional<bool>    vertical;

    bool isEmpty() const
    {
        return !fontFamily.has_value() && !fontPointSize.has_value()
            && !fontWeight.has_value() && !italic.has_value() && !fillColor.has_value()
            && !outlineColor.has_value() && !outlineWidth.has_value()
            && !shadowColor.has_value() && !shadowBlur.has_value()
            && !boxEnabled.has_value() && !boxColor.has_value()
            && !hAlign.has_value() && !vAlign.has_value() && !anchor.has_value()
            && !lineSpacing.has_value() && !letterSpacing.has_value()
            && !maxWidthRatio.has_value() && !rotationDeg.has_value()
            && !scale.has_value() && !opacity.has_value() && !vertical.has_value();
    }

    /// diff を base に上書き適用した結果を返す
    static SubtitleStyle apply(const SubtitleStyle& base, const SubtitleStyleDiff& d)
    {
        SubtitleStyle s = base;
        if (d.fontFamily)    s.fontFamily = *d.fontFamily;
        if (d.fontPointSize) s.fontPointSize = *d.fontPointSize;
        if (d.fontWeight)    s.fontWeight = *d.fontWeight;
        if (d.italic)        s.italic = *d.italic;
        if (d.fillColor)     s.fillColor = *d.fillColor;
        if (d.outlineColor)  s.outlineColor = *d.outlineColor;
        if (d.outlineWidth)  s.outlineWidth = *d.outlineWidth;
        if (d.shadowColor)   s.shadowColor = *d.shadowColor;
        if (d.shadowBlur)    s.shadowBlur = *d.shadowBlur;
        if (d.boxEnabled)    s.boxEnabled = *d.boxEnabled;
        if (d.boxColor)      s.boxColor = *d.boxColor;
        if (d.hAlign)        s.hAlign = HAlign(qBound(0, *d.hAlign, 2));
        if (d.vAlign)        s.vAlign = VAlign(qBound(0, *d.vAlign, 2));
        if (d.anchor)        s.anchor = *d.anchor;
        if (d.lineSpacing)   s.lineSpacing = *d.lineSpacing;
        if (d.letterSpacing) s.letterSpacing = *d.letterSpacing;
        if (d.maxWidthRatio) s.maxWidthRatio = *d.maxWidthRatio;
        if (d.rotationDeg)   s.rotationDeg = *d.rotationDeg;
        if (d.scale)         s.scale = *d.scale;
        if (d.opacity)       s.opacity = qBound(0.0, *d.opacity, 1.0);
        if (d.vertical)      s.vertical = *d.vertical;
        return s;
    }
};

} // namespace yave::subtitle
