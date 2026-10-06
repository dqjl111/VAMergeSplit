#pragma once

#include <QWidget>
#include <QVector>

class QProcess;

// 音频波形控件：绘制 min/max 峰值波形 + 播放游标，支持点击/拖动跳转
class WaveformWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget *parent = nullptr);

    void setWaveform(QVector<qint16> mins, QVector<qint16> maxs, double duration);
    void clear();
    void setMessage(const QString &message); // 加载中 / 失败提示
    void setPosition(double seconds);        // 播放游标

    // 缩放与平移（zoom 1.0 = 整个波形可见）
    void setZoom(double factor);
    double zoom() const { return m_zoom; }
    void setViewStart(double seconds);          // 可见窗口左缘
    void setViewStartFraction(double fraction); // 0~1 映射到可平移范围
    double viewStart() const { return m_viewStart; }
    double visibleDuration() const;
    double maxViewStart() const;

signals:
    void seekRequested(double seconds);
    void viewChanged(); // 缩放或平移发生变化

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    double secondsFromX(int x) const;

    QVector<qint16> m_mins;
    QVector<qint16> m_maxs;
    double m_duration = 0;
    double m_position = 0;
    QString m_message;
    double m_zoom = 1.0;    // 水平缩放倍数
    double m_viewStart = 0; // 可见窗口左缘（秒）
};

// 用 ffmpeg 把音频解码为低采样率单声道 PCM，并统计峰值
class WaveformLoader : public QObject {
    Q_OBJECT
public:
    explicit WaveformLoader(QObject *parent = nullptr);
    ~WaveformLoader() override;

    void load(const QString &ffmpegPath, const QString &filePath, double duration);
    void cancel();

signals:
    void finished(const QVector<qint16> &mins, const QVector<qint16> &maxs, double duration);
    void failed(const QString &error);

private:
    QProcess *m_process = nullptr;
    double m_duration = 0;
};
