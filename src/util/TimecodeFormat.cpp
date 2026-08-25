#include "TimecodeFormat.h"
#include <QRegularExpression>
#include <cmath>

namespace yave {

QString formatTimecode(int64_t frame, const Rational& tb, bool dropFrame)
{
    if (frame < 0) {
        frame = 0;
    }

    // fps を算出 (tb.den / tb.num の四捨五入)
    int fps = 30;
    if (tb.num > 0) {
        fps = std::round(double(tb.den) / double(tb.num));
    }

    int64_t h = 0, m = 0, s = 0, f = 0;

    if (dropFrame && (fps == 30 || fps == 60)) {
        if (fps == 30) {
            // NTSC drop frame rule: drop 2 frames per minute except every 10th minute.
            // 10 minutes = 17982 frames
            int64_t m10 = frame / 17982;
            int64_t d = frame % 17982;
            int64_t m1 = 0;
            if (d >= 1800) {
                m1 = 1 + (d - 1800) / 1798;
                d = (d - 1800) % 1798 + 2;
            }
            int64_t totalMinutes = m10 * 10 + m1;
            h = totalMinutes / 60;
            m = totalMinutes % 60;
            s = d / 30;
            f = d % 30;
        } else { // fps == 60
            // 59.94 fps drop frame rule: drop 4 frames per minute except every 10th minute.
            // 10 minutes = 35964 frames
            int64_t m10 = frame / 35964;
            int64_t d = frame % 35964;
            int64_t m1 = 0;
            if (d >= 3600) {
                m1 = 1 + (d - 3600) / 3596;
                d = (d - 3600) % 3596 + 4;
            }
            int64_t totalMinutes = m10 * 10 + m1;
            h = totalMinutes / 60;
            m = totalMinutes % 60;
            s = d / 60;
            f = d % 60;
        }
        return QStringLiteral("%1:%2:%3;%4")
            .arg(h, 2, 10, QLatin1Char('0'))
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'))
            .arg(f, 2, 10, QLatin1Char('0'));
    } else {
        int64_t totalSeconds = frame / fps;
        f = frame % fps;
        h = totalSeconds / 3600;
        m = (totalSeconds % 3600) / 60;
        s = totalSeconds % 60;

        return QStringLiteral("%1:%2:%3:%4")
            .arg(h, 2, 10, QLatin1Char('0'))
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'))
            .arg(f, 2, 10, QLatin1Char('0'));
    }
}

std::optional<int64_t> parseTimecode(const QString& s, const Rational& tb)
{
    static const QRegularExpression regex(QStringLiteral("^(\\d{1,2}):(\\d{1,2}):(\\d{1,2})([:;])(\\d{1,2})$"));
    auto match = regex.match(s);
    if (!match.hasMatch()) {
        return std::nullopt;
    }

    int64_t h = match.captured(1).toLongLong();
    int64_t m = match.captured(2).toLongLong();
    int64_t sec = match.captured(3).toLongLong();
    QString sep = match.captured(4);
    int64_t f = match.captured(5).toLongLong();

    int fps = 30;
    if (tb.num > 0) {
        fps = std::round(double(tb.den) / double(tb.num));
    }

    bool isDrop = (sep == QStringLiteral(";"));

    if (isDrop && (fps == 30 || fps == 60)) {
        int64_t totalMinutes = h * 60 + m;
        int64_t frames = (h * 3600 + m * 60 + sec) * fps + f;
        if (fps == 30) {
            int64_t dropped = 2 * (totalMinutes - totalMinutes / 10);
            frames -= dropped;
        } else { // fps == 60
            int64_t dropped = 4 * (totalMinutes - totalMinutes / 10);
            frames -= dropped;
        }
        return frames;
    } else {
        int64_t frames = (h * 3600 + m * 60 + sec) * fps + f;
        return frames;
    }
}

} // namespace yave
