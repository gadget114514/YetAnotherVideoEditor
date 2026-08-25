#include "SrtWriter.h"
#include "../SubtitleClip.h"
#include "../SubtitleText.h"

#include "../../core/Track.h"

#include <QFile>
#include <QSaveFile>

#include <algorithm>

namespace yave::subtitle {

namespace {

QString formatTimecode(double seconds)
{
    if (seconds < 0.0)
        seconds = 0.0;
    const int totalMs = int(seconds * 1000.0 + 0.5);
    const int h  = totalMs / 3600000;
    const int m  = (totalMs % 3600000) / 60000;
    const int s  = (totalMs % 60000) / 1000;
    const int ms = totalMs % 1000;
    return QStringLiteral("%1:%2:%3,%4")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'))
        .arg(ms, 3, 10, QLatin1Char('0'));
}

QString buildSrt(const Track& track, const Rational& tb, QStringList* warningsOut)
{
    // 字幕クリップのみを対象にする
    std::vector<const SubtitleClip*> subs;
    for (const auto& c : track.clips()) {
        if (c->type() == ClipType::Subtitle)
            subs.push_back(static_cast<const SubtitleClip*>(c.get()));
        else if (warningsOut && c->range().duration > 0)
            warningsOut->append(QObject::tr("Non-subtitle clip '%1' was skipped.")
                                    .arg(c->name()));
    }

    std::sort(subs.begin(), subs.end(),
              [](const SubtitleClip* a, const SubtitleClip* b) {
                  return a->range().start < b->range().start;
              });

    QString out;
    int index = 1;
    for (const SubtitleClip* sc : subs) {
        const TimeRange r = sc->range();
        const double startSec = framesToSeconds(r.start, tb);
        const double endSec   = framesToSeconds(r.end(), tb);

        out += QString::number(index++) + QStringLiteral("\n");
        out += formatTimecode(startSec) + QStringLiteral(" --> ")
             + formatTimecode(endSec) + QStringLiteral("\n");
        out += sc->text().toSrtMarkup() + QStringLiteral("\n\n");

        if (sc->hasMissingEffects() && warningsOut)
            warningsOut->append(
                QObject::tr("Clip '%1' has effects that cannot be stored in SRT; they will be lost.")
                    .arg(sc->name()));
    }
    return out;
}

bool writeTextAtomic(const QString& path, const QString& text, QString* errorOut)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }
    const QByteArray utf8 = text.toUtf8();
    if (file.write(utf8) != utf8.size()) {
        if (errorOut)
            *errorOut = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (errorOut)
            *errorOut = file.errorString();
        return false;
    }
    return true;
}

} // anonymous namespace

bool SrtWriter::write(const Track& track, const Rational& tb, const QString& path,
                      QStringList* warningsOut)
{
    return writeTextAtomic(path, buildSrt(track, tb, warningsOut), nullptr);
}

QString SrtWriter::writeToString(const Track& track, const Rational& tb,
                                 QStringList* warningsOut)
{
    return buildSrt(track, tb, warningsOut);
}

// ===========================================================================
//  VttParser: SRT パーサの互換パスを使う ('.' ミリ秒区切りも対応済み)
// ===========================================================================

SrtParseResult VttParser::parseText(const QString& text)
{
    QString stripped = text;
    // WEBVTT ヘッダ行を除去
    if (stripped.startsWith(QLatin1String("WEBVTT"), Qt::CaseInsensitive)) {
        const int eol = stripped.indexOf(QLatin1Char('\n'));
        stripped = (eol < 0) ? QString() : stripped.mid(eol + 1);
    }
    return SrtParser::parseText(stripped);
}

SrtParseResult VttParser::parseFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        SrtParseResult r;
        r.warnings.append(QObject::tr("Cannot open file: %1").arg(path));
        return r;
    }
    return VttParser::parseText(QString::fromUtf8(file.readAll()));
}

} // namespace yave::subtitle
