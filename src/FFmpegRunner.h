#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QProcess;

struct FfmpegTask {
    QString label;
    QStringList args;     // ffmpeg 可执行文件之后的参数
    double duration = 0;  // 预期输出时长（秒），用于进度计算
};

// 顺序执行一组 ffmpeg 任务，解析 stderr 中的 time= 上报总体进度
class FFmpegRunner : public QObject {
    Q_OBJECT
public:
    explicit FFmpegRunner(QObject *parent = nullptr);
    ~FFmpegRunner() override;

    void start(const QString &ffmpegPath, const QVector<FfmpegTask> &tasks);
    void cancel();
    bool isRunning() const { return m_process != nullptr; }

signals:
    void taskStarted(int index, int total, const QString &label);
    void progressChanged(double fraction); // 0.0 ~ 1.0
    void logLine(const QString &line);
    void allFinished(bool success, const QString &message);

private:
    void startNext();
    void onReadyRead();
    void onFinished(int exitCode);

    QString m_ffmpegPath;
    QProcess *m_process = nullptr;
    QVector<FfmpegTask> m_tasks;
    int m_current = -1;
    double m_totalDuration = 0;
    double m_completedDuration = 0;
    double m_currentTime = 0;
    QString m_buffer;
    bool m_cancelled = false;
};
