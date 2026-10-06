#pragma once

#include <QWidget>

#include "MediaInfo.h"

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QMediaPlayer;
class QAudioOutput;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QSpinBox;
class QStackedWidget;
class QTableView;
class QVideoWidget;

class FFmpegRunner;
class SegmentModel;
class WaveformLoader;
class WaveformWidget;

// 分割页：媒体预览 + 平均分割 + 片段表格微调 + JSON 导入导出 + 调用 ffmpeg 执行
class SplitWidget : public QWidget {
    Q_OBJECT
public:
    explicit SplitWidget(QWidget *parent = nullptr);

    void setFfmpegPaths(const QString &ffmpeg, const QString &ffprobe);

private slots:
    void openMediaFile();
    void doAverageSplit();
    void doAlignAsMinimax();
    void setStartFromPlayer();
    void setEndFromPlayer();
    void splitAtPlayerPosition();
    void removeSelectedRow();
    void moveSelectedRow(int delta);
    void exportJson();
    void importJson();
    void importJsonSegOnly();
    void startSplit();
    void cancelSplit();
    void onRowDoubleClicked(const QModelIndex &index);
    void jumpToTimeInput();
    void jumpToFrameInput();
    void playSelectedSegment();

private:
    bool loadMedia(const QString &filePath);
    void updateTransportTime(qint64 positionMs);
    void setControlsEnabled(bool enabled);
    void appendLog(const QString &line);
    QVector<struct FfmpegTask> buildTasks(const QString &outputDir, bool reencode) const;
    QString outputFileFor(int index, int total, const QString &outputDir) const;
    int selectedRow() const;
    void importJsonHelper(bool checkSource);

    QString m_ffmpegPath;
    QString m_ffprobePath;

    QString m_filePath;
    MediaInfo m_mediaInfo;
    bool m_primeFirstFrame = false; // 加载视频后等待首帧渲染再暂停
    bool m_playingSegment = false;  // 正在播放选中片段（到达片段终点自动暂停）
    qint64 m_segmentEndMs = 0;

    // 播放器
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    QWidget *m_videoPage = nullptr;             // 视频页：画面 + 下方可选波形
    QStackedWidget *m_previewStack = nullptr;
    QWidget *m_waveformPage = nullptr;          // 音频页：大波形 + 缩放/平移控制
    WaveformWidget *m_waveform = nullptr;       // 纯音频的大波形页
    WaveformWidget *m_waveformVideo = nullptr;   // 视频下方的紧凑波形
    QWidget *m_waveVideoBox = nullptr;           // 紧凑波形 + 缩放/平移控制条容器
    QSlider *m_waveVideoZoomSlider = nullptr;
    QSlider *m_waveVideoPanSlider = nullptr;
    bool m_updatingWaveVideoView = false;
    QCheckBox *m_waveformToggle = nullptr;       // “显示波形”开关（仅视频含音频时可见）
    QSlider *m_waveZoomSlider = nullptr;         // 波形缩放（1x ~ 200x）
    QSlider *m_wavePanSlider = nullptr;          // 缩放后的波形平移
    bool m_updatingWaveView = false;            // 防止缩放/平移滑块联动循环
    WaveformLoader *m_waveformLoader = nullptr;
    QSlider *m_positionSlider = nullptr;
    QLabel *m_timeLabel = nullptr;
    QPushButton *m_playButton = nullptr;
    QLineEdit *m_jumpTimeEdit = nullptr;
    QSpinBox *m_jumpFrameSpin = nullptr;
    QCheckBox *m_keepContinuousCheck = nullptr; // “保持连续”：设起点/终点时同步相邻段
    QPushButton *m_jumpFrameButton = nullptr;

    // 片段
    SegmentModel *m_model = nullptr;
    QTableView *m_table = nullptr;
    QSpinBox *m_countSpin = nullptr;

    // 输出
    QLineEdit *m_outputDirEdit = nullptr;
    QLineEdit *m_namePatternEdit = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPlainTextEdit *m_logView = nullptr;
    QSpinBox* m_patchFrame = nullptr;
    QLabel *m_patchFrameLabel = nullptr;

    FFmpegRunner *m_runner = nullptr;
    QLabel *m_infoLabel = nullptr;
};
