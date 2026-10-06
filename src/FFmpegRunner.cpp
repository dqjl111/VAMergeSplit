#include "FFmpegRunner.h"

#include <QProcess>
#include <QRegularExpression>

FFmpegRunner::FFmpegRunner(QObject *parent)
    : QObject(parent)
{
}

FFmpegRunner::~FFmpegRunner()
{
    cancel();
}

void FFmpegRunner::start(const QString &ffmpegPath, const QVector<FfmpegTask> &tasks)
{
    if (isRunning() || tasks.isEmpty())
        return;

    m_ffmpegPath = ffmpegPath;
    m_tasks = tasks;
    m_current = -1;
    m_completedDuration = 0;
    m_cancelled = false;

    m_totalDuration = 0;
    for (const FfmpegTask &t : m_tasks)
        m_totalDuration += t.duration;
    if (m_totalDuration <= 0.0)
        m_totalDuration = double(m_tasks.size());

    emit progressChanged(0.0);
    startNext();
}

void FFmpegRunner::cancel()
{
    if (!m_process)
        return;
    m_cancelled = true;
    m_process->kill();
}

void FFmpegRunner::startNext()
{
    ++m_current;
    if (m_current >= m_tasks.size()) {
        emit progressChanged(1.0);
        emit allFinished(true, QStringLiteral("全部任务完成"));
        return;
    }

    const FfmpegTask &task = m_tasks[m_current];
    m_currentTime = 0;
    m_buffer.clear();

    m_process = new QProcess(this);
    m_process->setProgram(m_ffmpegPath);
    m_process->setArguments(task.args);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyRead, this, &FFmpegRunner::onReadyRead);
    connect(m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus) { onFinished(exitCode); });

    emit taskStarted(m_current, m_tasks.size(), task.label);
    emit logLine(QStringLiteral("> ffmpeg %1").arg(task.args.join(QLatin1Char(' '))));
    m_process->start();
}

void FFmpegRunner::onReadyRead()
{
    if (!m_process)
        return;
    m_buffer += QString::fromLocal8Bit(m_process->readAll());

    // 处理完整行输出日志
    int idx;
    while ((idx = m_buffer.indexOf(QLatin1Char('\n'))) >= 0) {
        QString line = m_buffer.left(idx).trimmed();
        m_buffer.remove(0, idx + 1);
        if (!line.isEmpty())
            emit logLine(line);
    }

    // 从残留缓冲区解析 time=（进度行以 \r 刷新，可能不带换行）
    static const QRegularExpression re(QStringLiteral("time=(\\d+):(\\d+):(\\d+(?:\\.\\d+)?)"));
    QRegularExpressionMatchIterator it = re.globalMatch(m_buffer);
    double lastTime = -1;
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const double h = m.captured(1).toDouble();
        const double min = m.captured(2).toDouble();
        const double s = m.captured(3).toDouble();
        lastTime = h * 3600.0 + min * 60.0 + s;
    }
    if (lastTime >= 0) {
        m_currentTime = lastTime;
        const FfmpegTask &task = m_tasks[m_current];
        const double taskDur = task.duration > 0 ? task.duration : 1.0;
        double fraction = (m_completedDuration + qMin(m_currentTime, taskDur)) / m_totalDuration;
        emit progressChanged(qBound(0.0, fraction, 1.0));
        // 丢弃已处理内容，防止缓冲区无限增长
        if (m_buffer.size() > 4096)
            m_buffer = m_buffer.right(256);
    }
}

void FFmpegRunner::onFinished(int exitCode)
{
    if (m_process) {
        const QString rest = m_process->readAll();
        if (!rest.isEmpty())
            emit logLine(QString::fromLocal8Bit(rest.toLocal8Bit()).trimmed());
        m_process->deleteLater();
        m_process = nullptr;
    }

    if (m_cancelled) {
        emit allFinished(false, QStringLiteral("已取消"));
        return;
    }
    if (exitCode != 0) {
        emit allFinished(false,
            QStringLiteral("任务 \"%1\" 失败，ffmpeg 退出码 %2")
                .arg(m_tasks[m_current].label).arg(exitCode));
        return;
    }

    m_completedDuration += m_tasks[m_current].duration > 0
        ? m_tasks[m_current].duration : 1.0;
    emit progressChanged(m_completedDuration / m_totalDuration);
    startNext();
}
