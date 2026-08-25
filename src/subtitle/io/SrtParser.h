#pragma once

#include "../../core/Rational.h"

#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace yave {

class Track;
class Clip;
class SubtitleStylePresetTable;

} // namespace yave

namespace yave::subtitle {

class SubtitleClip;
struct SubtitleGlyphRun;

/// SRT の 1 キュー
struct SrtCue
{
    int    index = 0;
    double startSeconds = 0.0;
    double endSeconds   = 0.0;
    QString rawText;              ///< インラインタグを含む生テキスト
};

struct SrtParseResult
{
    std::vector<SrtCue> cues;
    QStringList         warnings;   ///< 重なり、逆転、パース失敗行など
    bool                ok = false;
};

/// SRT パーサ。
///
/// エンコーディングは BOM 判定 -> UTF-8 検証 -> システムロケール(日本語環境なら
/// Shift_JIS)の順で試す。日本語圏の SRT は Shift_JIS で配布されていることが
/// 依然として多いため、この判定が実務上もっとも重要。
class SrtParser
{
public:
    static SrtParseResult parseFile(const QString& path);
    static SrtParseResult parseText(const QString& text);

private:
    static bool parseTimecodeLine(const QString& line, double* start, double* end);
};

/// 秒 -> フレーム変換しつつ SubtitleClip 群へ変換する。
/// 開始は Floor、終了は Ceil にして「表示されるべき瞬間が確実に含まれる」ようにする。
std::vector<std::shared_ptr<SubtitleClip>> convertCuesToClips(
    const SrtParseResult& parsed,
    const Rational& timebase,
    const QString& stylePresetId,
    QStringList* warningsOut);

} // namespace yave::subtitle

// SrtParser.cpp 内部実装で使うための公開ヘルパ (テストからも利用)
bool parseSrtTimecode(const QString& line, double* start, double* end);
