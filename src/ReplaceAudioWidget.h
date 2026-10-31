#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

class FFmpegRunner;

// 音频替换页：用一段新的音频替换视频中原有的音轨（视频流无损直拷）
class ReplaceAudioWidget : public QWidget {
    Q_OBJECT
public:
    explicit ReplaceAudioWidget(QWidget *parent = nullptr);

    void setFfmpegPaths(const QString &ffmpeg, const QString &ffprobe);

private slots:
    void browseVideo();
    void browseAudio();
    void browseOutput();
    void startReplace();
    void cancelReplace();

private:
    void appendLog(const QString &line);

    QString m_ffmpegPath;
    QString m_ffprobePath;

    QLineEdit *m_videoEdit = nullptr;
    QLineEdit *m_audioEdit = nullptr;
    QLineEdit *m_outputEdit = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QLabel *m_infoLabel = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPlainTextEdit *m_logView = nullptr;

    FFmpegRunner *m_runner = nullptr;
};
