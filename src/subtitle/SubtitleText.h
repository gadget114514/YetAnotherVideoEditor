#pragma once

#include <QColor>
#include <QString>

#include <optional>
#include <vector>

namespace yave::subtitle {

/// 文字範囲に対する装飾。範囲は UTF-16 コードユニットのインデックス(QString と同じ)。
struct TextSpan
{
    int  start  = 0;
    int  length = 0;
    std::optional<bool>     bold;
    std::optional<bool>     italic;
    std::optional<bool>     underline;
    std::optional<QColor>   color;
    std::optional<QString>  fontFamily;
    std::optional<double>   sizeScale;      ///< 基準サイズに対する倍率
    QString                 ruby;           ///< ルビ(振り仮名)。空なら無し
};

/// プレーン文字列 + リッチスパン。
class SubtitleText
{
public:
    QString plain() const { return plain_; }
    void    setPlain(const QString& s)
    {
        plain_ = s;
        normalizeSpans();
    }

    const std::vector<TextSpan>& spans() const { return spans_; }
    void addSpan(const TextSpan& s);
    void clearSpans() { spans_.clear(); }

    /// SRT のインラインタグ (<b> <i> <u> <font color=...>) をパースして spans に変換
    static SubtitleText fromSrtMarkup(const QString& markup);

    /// spans を SRT インラインタグへ戻す
    QString toSrtMarkup() const;

    /// 改行で分割した行数
    int lineCount() const;

    bool operator==(const SubtitleText& o) const
    { return plain_ == o.plain_ && spans_.size() == o.spans_.size(); }

private:
    void normalizeSpans();

    QString                plain_;
    std::vector<TextSpan>  spans_;
};

} // namespace yave::subtitle
