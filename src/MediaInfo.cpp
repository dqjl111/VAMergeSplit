#include "MediaInfo.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

static double parseRational(const QString &s)
{
    const QStringList parts = s.split(QLatin1Char('/'));
    if (parts.size() == 2) {
        const double den = parts[1].toDouble();
        if (den > 0.0)
            return parts[0].toDouble() / den;
        return 0.0;
    }
    return s.toDouble();
}

MediaInfo probeMedia(const QString &ffprobePath, const QString &filePath)
{
    MediaInfo info;

    QProcess proc;
    proc.setProgram(ffprobePath);
    proc.setArguments({
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-print_format"), QStringLiteral("json"),
        QStringLiteral("-show_format"),
        QStringLiteral("-show_streams"),
        filePath
    });
    proc.start();
    if (!proc.waitForFinished(15000)) {
        info.error = QStringLiteral("ffprobe 执行超时");
        proc.kill();
        proc.waitForFinished(1000);
        return info;
    }
    if (proc.exitCode() != 0) {
        info.error = QString::fromLocal8Bit(proc.readAllStandardError()).trimmed();
        if (info.error.isEmpty())
            info.error = QStringLiteral("ffprobe 返回错误码 %1").arg(proc.exitCode());
        return info;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(proc.readAllStandardOutput());
    if (!doc.isObject()) {
        info.error = QStringLiteral("ffprobe 输出解析失败");
        return info;
    }

    const QJsonObject root = doc.object();
    const QJsonObject format = root.value(QStringLiteral("format")).toObject();
    info.duration = format.value(QStringLiteral("duration")).toString().toDouble();

    const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
    for (const QJsonValue &v : streams) {
        const QJsonObject s = v.toObject();
        const QString type = s.value(QStringLiteral("codec_type")).toString();
        if (type == QLatin1String("video") && !info.hasVideo) {
            // 忽略封面图等 attached_pic
            if (s.value(QStringLiteral("disposition")).toObject()
                    .value(QStringLiteral("attached_pic")).toInt() != 0)
                continue;
            info.hasVideo = true;
            info.videoCodec = s.value(QStringLiteral("codec_name")).toString();
            info.width = s.value(QStringLiteral("width")).toInt();
            info.height = s.value(QStringLiteral("height")).toInt();
            double fps = parseRational(s.value(QStringLiteral("avg_frame_rate")).toString());
            if (fps <= 0.0)
                fps = parseRational(s.value(QStringLiteral("r_frame_rate")).toString());
            info.fps = fps;
            if (info.duration <= 0.0)
                info.duration = s.value(QStringLiteral("duration")).toString().toDouble();
        } else if (type == QLatin1String("audio") && !info.hasAudio) {
            info.hasAudio = true;
            info.audioCodec = s.value(QStringLiteral("codec_name")).toString();
            if (info.duration <= 0.0)
                info.duration = s.value(QStringLiteral("duration")).toString().toDouble();
        }
    }

    if (info.duration <= 0.0) {
        info.error = QStringLiteral("无法获取媒体时长");
        return info;
    }
    if (!info.hasVideo && !info.hasAudio) {
        info.error = QStringLiteral("文件不包含音视频流");
        return info;
    }

    info.ok = true;
    return info;
}

QList<double> probeKeyframeTimes(const QString &ffprobePath, const QString &filePath)
{
    QList<double> times;

    QProcess proc;
    proc.setProgram(ffprobePath);
    proc.setArguments({
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-select_streams"), QStringLiteral("v:0"),
        QStringLiteral("-skip_frame"), QStringLiteral("nokey"),
        QStringLiteral("-show_entries"), QStringLiteral("frame=pts_time"),
        QStringLiteral("-of"), QStringLiteral("csv=p=0"),
        filePath
    });
    proc.start();
    if (!proc.waitForFinished(60000)) {
        proc.kill();
        proc.waitForFinished(1000);
        return times;
    }
    if (proc.exitCode() != 0)
        return times;

    // csv=p=0 每行一个时间值（可能带尾随逗号）
    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        bool ok = false;
        const double t = line.section(QLatin1Char(','), 0, 0).trimmed().toDouble(&ok);
        if (ok)
            times.append(t);
    }
    std::sort(times.begin(), times.end());
    return times;
}
