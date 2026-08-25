#pragma once

#include "SrtParser.h"

#include <QString>
#include <QStringList>

namespace yave {

class Track;

} // namespace yave

namespace yave::subtitle {

/// SRT 書き出し。
///
/// エフェクトとスタイルは SRT では表現できないため失われる(警告を出す)。
class SrtWriter
{
public:
    /// 指定トラックの SubtitleClip を SRT として書き出す。
    static bool write(const Track& track, const Rational& tb, const QString& path,
                      QStringList* warningsOut);

    /// メモリ上へ書き出す (テスト用)。
    static QString writeToString(const Track& track, const Rational& tb,
                                 QStringList* warningsOut);
};

/// WebVTT パーサ。SRT パーサの互換パスで読む (タイムコードが '.' 区切りでも
/// ',' 区切りでも受け付ける)。
class VttParser
{
public:
    static SrtParseResult parseFile(const QString& path);
    static SrtParseResult parseText(const QString& text);
};

} // namespace yave::subtitle
