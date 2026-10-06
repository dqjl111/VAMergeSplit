#include "TimeUtils.h"

#include <QRegularExpression>

namespace TimeUtils {

double parseTime(const QString &text, bool *ok)
{
    bool localOk = false;
    const QString t = text.trimmed();

    // HH:MM:SS(.mmm) 或 MM:SS(.mmm)
    static const QRegularExpression re(
        R"(^(?:(\d+):)?([0-5]?\d):([0-5]?\d(?:\.\d+)?)$)");
    const QRegularExpressionMatch m = re.match(t);
    double result = 0.0;
    if (m.hasMatch()) {
        const double sec = m.captured(3).toDouble();
        const double min = m.captured(2).toDouble();
        const double hour = m.captured(1).isEmpty() ? 0.0 : m.captured(1).toDouble();
        result = hour * 3600.0 + min * 60.0 + sec;
        localOk = true;
    } else {
        result = t.toDouble(&localOk);
    }

    if (localOk && result < 0.0)
        localOk = false;

    if (ok)
        *ok = localOk;
    return localOk ? result : 0.0;
}

QString formatTime(double seconds)
{
    if (seconds < 0.0)
        seconds = 0.0;
    const qint64 totalMs = qint64(seconds * 1000.0 + 0.5);
    const qint64 ms = totalMs % 1000;
    const qint64 totalSec = totalMs / 1000;
    const qint64 sec = totalSec % 60;
    const qint64 min = (totalSec / 60) % 60;
    const qint64 hour = totalSec / 3600;
    return QStringLiteral("%1:%2:%3.%4")
        .arg(hour, 2, 10, QLatin1Char('0'))
        .arg(min, 2, 10, QLatin1Char('0'))
        .arg(sec, 2, 10, QLatin1Char('0'))
        .arg(ms, 3, 10, QLatin1Char('0'));
}

qint64 frameFromTime(double seconds, double fps)
{
    if (fps <= 0.0)
        return 0;
    return qint64(seconds * fps + 0.5);
}

double timeFromFrame(qint64 frame, double fps)
{
    if (fps <= 0.0)
        return 0.0;
    return double(frame) / fps;
}

} // namespace TimeUtils
