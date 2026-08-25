#include "SrtParser.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringConverter>
#include <QStringDecoder>

#include <algorithm>

namespace yave::subtitle {

// ===========================================================================
//  エンコーディング判定
// ===========================================================================

namespace {

/// BOM 判定 -> UTF-8 検証 -> システムロケール (日本語環境なら Shift_JIS)。
QString decodeBytes(const QByteArray& bytes, bool* okOut)
{
    *okOut = true;

    // (1) BOM
    if (bytes.startsWith(QByteArrayLiteral("\xEF\xBB\xBF")))
        return QString::fromUtf8(bytes.mid(3));
    if (bytes.startsWith(QByteArrayLiteral("\xFF\xFE")))
        return QString::fromUtf16(reinterpret_cast<const char16_t*>(bytes.constData()),
                                  int((bytes.size() - 2) / 2));
    if (bytes.startsWith(QByteArrayLiteral("\xFE\xFF"))) {
        // Big endian UTF-16: バイトスワップしてからデコード
        QByteArray swapped = bytes.mid(2);
        for (int i = 0; i + 1 < swapped.size(); i += 2)
            std::swap(swapped[i], swapped[i + 1]);
        return QString::fromUtf16(reinterpret_cast<const char16_t*>(swapped.constData()),
                                  int(swapped.size() / 2));
    }

    // (2) UTF-8 検証。不正バイト列を含めば UTF-8 ではない
    {
        QStringDecoder utf8(QStringConverter::Utf8);
        const QString s = utf8.decode(bytes);
        if (!utf8.hasError())
            return s;
    }

    // (3) Shift_JIS フォールバック
    {
        QStringDecoder sjis(QStringConverter::System);   // 日本語環境では Shift_JIS 相当
        const QString s = sjis.decode(bytes);
        if (!sjis.hasError())
            return s;
    }

    *okOut = false;
    return QString::fromUtf8(bytes);
}

} // anonymous namespace

// ===========================================================================
//  タイムコード
// ===========================================================================

bool SrtParser::parseTimecodeLine(const QString& line, double* start, double* end)
{
    return ::parseSrtTimecode(line, start, end);
}

// ===========================================================================
//  パース本体
// ===========================================================================

SrtParseResult SrtParser::parseText(const QString& text)
{
    SrtParseResult result;

    // CRLF / CR を LF へ正規化し、末尾の余分な空行を除去
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    enum class Section { Index, Timecode, Body };
    Section section = Section::Index;

    int      currentIndex = 0;
    double   startSec = 0.0, endSec = 0.0;
    QString  body;

    auto flushCue = [&]() {
        if (section != Section::Body)
            return;
        SrtCue cue;
        cue.index        = currentIndex;
        cue.startSeconds = startSec;
        cue.endSeconds   = endSec;
        cue.rawText      = body.trimmed();
        if (cue.endSeconds <= cue.startSeconds) {
            result.warnings.append(
                QObject::tr("Cue #%1 has zero or negative duration; skipped.")
                    .arg(cue.index));
        } else if (cue.rawText.isEmpty()) {
            result.warnings.append(
                QObject::tr("Cue #%1 has empty text; skipped.").arg(cue.index));
        } else {
            result.cues.push_back(cue);
        }
        section = Section::Index;
        body.clear();
    };

    const QStringList lines = normalized.split(QLatin1Char('\n'));
    for (const QString& rawLine : lines) {
        const QString line = rawLine;   // 空白保持 (ボディ内のインデント用)

        switch (section) {
        case Section::Index: {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty())
                continue;
            bool isNum = false;
            const int idx = trimmed.toInt(&isNum);
            if (isNum && idx >= 0) {
                flushCue();
                currentIndex = idx;
                section = Section::Timecode;
            } else if (parseTimecodeLine(trimmed, &startSec, &endSec)) {
                // インデックス行を省略した SRT も許容する
                currentIndex++;
                section = Section::Body;
            } else if (!trimmed.startsWith(QLatin1String("WEBVTT"), Qt::CaseInsensitive)) {
                // WEBVTT ヘッダは無視して読み飛ばす (互換パース)
                flushCue();
            }
            break;
        }
        case Section::Timecode:
            if (parseTimecodeLine(line, &startSec, &endSec)) {
                section = Section::Body;
                body.clear();
            } else {
                // 不正行: インデックスへ巻き戻す
                result.warnings.append(
                    QObject::tr("Malformed timecode line: %1").arg(line.trimmed()));
                section = Section::Index;
            }
            break;
        case Section::Body:
            if (line.trimmed().isEmpty()) {
                flushCue();
            } else {
                if (!body.isEmpty())
                    body += QLatin1Char('\n');
                body += line;
            }
            break;
        }
    }
    flushCue();

    // 重なり検査 (自動修正はしない。UI に提示して判断させる)
    for (size_t i = 1; i < result.cues.size(); ++i) {
        if (result.cues[i].startSeconds < result.cues[i - 1].endSeconds)
            result.warnings.append(
                QObject::tr("Cue #%1 overlaps the previous cue.")
                    .arg(result.cues[i].index));
    }

    result.ok = !result.cues.empty();
    return result;
}

SrtParseResult SrtParser::parseFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        SrtParseResult r;
        r.warnings.append(QObject::tr("Cannot open file: %1").arg(path));
        return r;
    }

    bool decoded = false;
    const QString text = decodeBytes(file.readAll(), &decoded);
    if (!decoded) {
        SrtParseResult r;
        r.warnings.append(QObject::tr("Unsupported text encoding: %1").arg(path));
        return r;
    }
    return parseText(text);
}

} // namespace yave::subtitle

// ===========================================================================
//  グローバルスコープのタイムコードパーサ (テストからも利用)
// ===========================================================================

bool parseSrtTimecode(const QString& line, double* start, double* end)
{
    static const QRegularExpression re(
        QStringLiteral("(\\d{1,2}):(\\d{1,2}):(\\d{1,2})[,.](\\d{1,3})"
                       "\\s*-->\\s*(\\d{1,2}):(\\d{1,2}):(\\d{1,2})[,.](\\d{1,3})"));

    const auto m = re.match(line.trimmed());
    if (!m.hasMatch())
        return false;

    auto toSecs = [](const QString& h, const QString& mi, const QString& s,
                     const QString& ms) {
        return double(h.toInt()) * 3600.0 + double(mi.toInt()) * 60.0
             + double(s.toInt()) + double(ms.rightJustified(3, u'0').toInt()) / 1000.0;
    };

    *start = toSecs(m.captured(1), m.captured(2), m.captured(3), m.captured(4));
    *end   = toSecs(m.captured(5), m.captured(6), m.captured(7), m.captured(8));
    return true;
}
