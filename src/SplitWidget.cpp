#include "SplitWidget.h"

#include "FFmpegRunner.h"
#include "SegmentModel.h"
#include "TimeUtils.h"
#include "WaveformWidget.h"

#include <QAudioOutput>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QVideoSink>
#include <QVideoWidget>

SplitWidget::SplitWidget(QWidget *parent)
    : QWidget(parent)
{
    // ---------- 顶部：文件选择 ----------
    auto *openButton = new QPushButton(QStringLiteral("打开媒体文件..."), this);
    m_infoLabel = new QLabel(QStringLiteral("未加载文件"), this);
    m_infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *topLayout = new QHBoxLayout;
    topLayout->addWidget(openButton);
    topLayout->addWidget(m_infoLabel, 1);

    // ---------- 左侧：播放器 ----------
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setMinimumSize(400, 260);
    m_player->setVideoOutput(m_videoWidget);

    // 预览区：视频显示画面（下方可叠加紧凑波形），纯音频显示大波形
    m_waveform = new WaveformWidget(this);
    m_waveformVideo = new WaveformWidget(this);
    m_waveformVideo->setFixedHeight(80);
    m_waveVideoZoomSlider = new QSlider(Qt::Horizontal, this);
    m_waveVideoZoomSlider->setRange(10, 2000); // 值/10 = 缩放倍数
    m_waveVideoZoomSlider->setValue(10);
    m_waveVideoZoomSlider->setToolTip(QStringLiteral("波形缩放，也可直接在波形上滚动鼠标滚轮"));
    m_waveVideoPanSlider = new QSlider(Qt::Horizontal, this);
    m_waveVideoPanSlider->setRange(0, 1000);
    m_waveVideoPanSlider->setValue(0);
    m_waveVideoPanSlider->setEnabled(false);
    m_waveVideoPanSlider->setToolTip(QStringLiteral("左右拖动查看缩放后的波形位置"));
    auto *waveVideoCtrlLayout = new QHBoxLayout;
    waveVideoCtrlLayout->setContentsMargins(0, 0, 0, 0);
    waveVideoCtrlLayout->addWidget(new QLabel(QStringLiteral("缩放"), this));
    waveVideoCtrlLayout->addWidget(m_waveVideoZoomSlider, 1);
    waveVideoCtrlLayout->addWidget(new QLabel(QStringLiteral("平移"), this));
    waveVideoCtrlLayout->addWidget(m_waveVideoPanSlider, 1);
    m_waveVideoBox = new QWidget(this);
    auto *waveVideoBoxLayout = new QVBoxLayout(m_waveVideoBox);
    waveVideoBoxLayout->setContentsMargins(0, 0, 0, 0);
    waveVideoBoxLayout->setSpacing(2);
    waveVideoBoxLayout->addWidget(m_waveformVideo);
    waveVideoBoxLayout->addLayout(waveVideoCtrlLayout);
    m_waveVideoBox->hide(); // 加载含音频的视频后由“显示波形”开关控制
    m_videoPage = new QWidget(this);
    auto *videoPageLayout = new QVBoxLayout(m_videoPage);
    videoPageLayout->setContentsMargins(0, 0, 0, 0);
    videoPageLayout->setSpacing(2);
    videoPageLayout->addWidget(m_videoWidget, 1);
    videoPageLayout->addWidget(m_waveVideoBox);

    // 音频页：大波形 + 缩放/平移控制条
    m_waveZoomSlider = new QSlider(Qt::Horizontal, this);
    m_waveZoomSlider->setRange(10, 2000); // 值/10 = 缩放倍数（1x ~ 200x）
    m_waveZoomSlider->setValue(10);
    m_waveZoomSlider->setToolTip(QStringLiteral("波形缩放，也可直接在波形上滚动鼠标滚轮"));
    m_wavePanSlider = new QSlider(Qt::Horizontal, this);
    m_wavePanSlider->setRange(0, 1000);
    m_wavePanSlider->setValue(0);
    m_wavePanSlider->setEnabled(false); // 缩放大于 1x 时才可平移
    m_wavePanSlider->setToolTip(QStringLiteral("左右拖动查看缩放后的波形位置"));
    auto *waveCtrlLayout = new QHBoxLayout;
    waveCtrlLayout->setContentsMargins(0, 0, 0, 0);
    waveCtrlLayout->addWidget(new QLabel(QStringLiteral("缩放"), this));
    waveCtrlLayout->addWidget(m_waveZoomSlider, 1);
    waveCtrlLayout->addWidget(new QLabel(QStringLiteral("平移"), this));
    waveCtrlLayout->addWidget(m_wavePanSlider, 1);
    m_waveformPage = new QWidget(this);
    auto *wavePageLayout = new QVBoxLayout(m_waveformPage);
    wavePageLayout->setContentsMargins(0, 0, 0, 0);
    wavePageLayout->setSpacing(2);
    wavePageLayout->addWidget(m_waveform, 1);
    wavePageLayout->addLayout(waveCtrlLayout);

    m_previewStack = new QStackedWidget(this);
    m_previewStack->addWidget(m_videoPage);
    m_previewStack->addWidget(m_waveformPage);
    m_previewStack->setCurrentWidget(m_videoPage);
    m_previewStack->setMinimumSize(400, 260);
    m_waveformLoader = new WaveformLoader(this);

    m_playButton = new QPushButton(QStringLiteral("播放"), this);
    m_positionSlider = new QSlider(Qt::Horizontal, this);
    m_timeLabel = new QLabel(QStringLiteral("00:00:00.000 / 00:00:00.000"), this);
    m_waveformToggle = new QCheckBox(QStringLiteral("显示波形"), this);
    m_waveformToggle->setChecked(true);
    m_waveformToggle->hide(); // 仅视频含音频时显示

    // 进度条独占一行；播放按钮、波形开关与时间/帧位置同在其下方一行
    auto *transportLayout = new QHBoxLayout;
    transportLayout->addWidget(m_positionSlider, 1);

    auto *timeLayout = new QHBoxLayout;
    timeLayout->addWidget(m_playButton);
    timeLayout->addWidget(m_waveformToggle);
    timeLayout->addStretch(1);
    timeLayout->addWidget(m_timeLabel);

    // 跳转：按时间 / 按帧号
    m_jumpTimeEdit = new QLineEdit(this);
    m_jumpTimeEdit->setPlaceholderText(QStringLiteral("如 83.5 或 00:01:23.500"));
    m_jumpTimeEdit->setMaximumWidth(170);
    m_jumpTimeEdit->setToolTip(QStringLiteral("支持格式：秒、MM:SS、HH:MM:SS.mmm"));
    auto *jumpTimeButton = new QPushButton(QStringLiteral("跳转"), this);
    m_jumpFrameSpin = new QSpinBox(this);
    m_jumpFrameSpin->setRange(0, 999999999);
    m_jumpFrameSpin->setAccelerated(true);
    m_jumpFrameSpin->setEnabled(false); // 加载视频后启用
    m_jumpFrameButton = new QPushButton(QStringLiteral("跳转"), this);
    m_jumpFrameButton->setEnabled(false);
    auto *jumpLayout = new QHBoxLayout;
    jumpLayout->addWidget(new QLabel(QStringLiteral("跳到时间:"), this));
    jumpLayout->addWidget(m_jumpTimeEdit);
    jumpLayout->addWidget(jumpTimeButton);
    jumpLayout->addSpacing(16);
    jumpLayout->addWidget(new QLabel(QStringLiteral("跳到帧:"), this));
    jumpLayout->addWidget(m_jumpFrameSpin);
    jumpLayout->addWidget(m_jumpFrameButton);
    jumpLayout->addStretch(1);

    auto *markStartButton = new QPushButton(QStringLiteral("设为起点"), this);
    auto *markEndButton = new QPushButton(QStringLiteral("设为终点"), this);
    auto *splitHereButton = new QPushButton(QStringLiteral("在播放位置拆段"), this);
    auto *playSegButton = new QPushButton(QStringLiteral("播放片段"), this);
    markStartButton->setToolTip(QStringLiteral("把当前播放位置写入选中行的开始时间"));
    markEndButton->setToolTip(QStringLiteral("把当前播放位置写入选中行的结束时间"));
    splitHereButton->setToolTip(QStringLiteral("把选中行在当前播放位置一分为二"));
    playSegButton->setToolTip(QStringLiteral("从选中片段的起点播放到终点后自动暂停"));
    m_keepContinuousCheck = new QCheckBox(QStringLiteral("保持连续"), this);
    m_keepContinuousCheck->setChecked(true);
    m_keepContinuousCheck->setToolTip(QStringLiteral(
        "勾选后：设为起点时同步更新上一段的终点；设为终点时同步更新下一段的起点"));
    auto *markLayout = new QHBoxLayout;
    markLayout->addWidget(markStartButton);
    markLayout->addWidget(markEndButton);
    markLayout->addWidget(splitHereButton);
    markLayout->addWidget(playSegButton);
    markLayout->addWidget(m_keepContinuousCheck);
    markLayout->addStretch(1);

    auto *playerLayout = new QVBoxLayout;
    playerLayout->addWidget(m_previewStack, 1);
    playerLayout->addLayout(transportLayout);
    playerLayout->addLayout(timeLayout);
    playerLayout->addLayout(jumpLayout);
    playerLayout->addLayout(markLayout);
    auto *playerBox = new QGroupBox(QStringLiteral("预览"), this);
    playerBox->setLayout(playerLayout);

    // ---------- 右侧：片段表格与操作 ----------
    auto *countLabel = new QLabel(QStringLiteral("分割段数:"), this);
    m_countSpin = new QSpinBox(this);
    m_countSpin->setRange(1, 999);
    m_countSpin->setValue(3);
    auto *avgButton = new QPushButton(QStringLiteral("平均分割"), this);
    auto *alignAsMinimaxButton = new QPushButton(QStringLiteral("按MinimaxH3对齐"), this);

    avgButton->setFocusPolicy(Qt::NoFocus);
    alignAsMinimaxButton->setFocusPolicy(Qt::NoFocus);

    auto *splitLayout = new QHBoxLayout;
    splitLayout->addWidget(countLabel);
    splitLayout->addWidget(m_countSpin);
    splitLayout->addWidget(avgButton);
    splitLayout->addWidget(alignAsMinimaxButton);
    splitLayout->addStretch(1);

    m_model = new SegmentModel(this);
    m_table = new QTableView(this);
    m_table->setModel(m_model);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *upButton = new QPushButton(QStringLiteral("上移"), this);
    auto *downButton = new QPushButton(QStringLiteral("下移"), this);
    auto *removeButton = new QPushButton(QStringLiteral("删除行"), this);
    auto *exportButton = new QPushButton(QStringLiteral("导出JSON"), this);
    auto *importButton = new QPushButton(QStringLiteral("导入JSON"), this);
    auto *importSegOnlyButton = new QPushButton(QStringLiteral("导入JSON(仅分段)"), this);
    auto *rowLayout = new QHBoxLayout;
    upButton->setFocusPolicy(Qt::NoFocus);
    downButton->setFocusPolicy(Qt::NoFocus);
    removeButton->setFocusPolicy(Qt::NoFocus);
    exportButton->setFocusPolicy(Qt::NoFocus);
    importButton->setFocusPolicy(Qt::NoFocus);
    importSegOnlyButton->setFocusPolicy(Qt::NoFocus);

    rowLayout->addWidget(upButton);
    rowLayout->addWidget(downButton);
    rowLayout->addWidget(removeButton);
    rowLayout->addSpacing(20);
    rowLayout->addWidget(exportButton);
    rowLayout->addWidget(importButton);
    rowLayout->addWidget(importSegOnlyButton);
    rowLayout->addStretch(1);

    // 输出设置
    m_outputDirEdit = new QLineEdit(this);
    auto *browseDirButton = new QPushButton(QStringLiteral("浏览..."), this);
    auto *dirLayout = new QHBoxLayout;
    dirLayout->addWidget(m_outputDirEdit, 1);
    dirLayout->addWidget(browseDirButton);

    m_namePatternEdit = new QLineEdit(QStringLiteral("part_%1"), this);
    m_namePatternEdit->setToolTip(QStringLiteral("%1 会被替换为片段序号，例如 part_%1 -> part_1"));
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QStringLiteral("无损直拷（快，切点对齐关键帧）"), false);
    m_modeCombo->addItem(QStringLiteral("重编码（慢，切点精确到帧）"), true);

    auto *outputForm = new QFormLayout;
    outputForm->addRow(QStringLiteral("输出目录:"), dirLayout);
    outputForm->addRow(QStringLiteral("命名规则:"), m_namePatternEdit);
    outputForm->addRow(QStringLiteral("输出模式:"), m_modeCombo);

    m_startButton = new QPushButton(QStringLiteral("开始分割"), this);
    m_startButton->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_cancelButton = new QPushButton(QStringLiteral("取消"), this);
    m_cancelButton->setEnabled(false);

    m_patchFrameLabel = new QLabel(QStringLiteral("向后补偿帧数："), this);
    m_patchFrameLabel->setVisible(false);
    m_patchFrame = new QSpinBox(this);

    m_patchFrame->setMinimum(0);
    m_patchFrame->setMaximum(100);
    m_patchFrame->setValue(0);
    m_patchFrame->setVisible(false);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 1000);
    m_progressBar->setValue(0);
    auto *runLayout = new QHBoxLayout;
    runLayout->addWidget(m_startButton);
    runLayout->addWidget(m_cancelButton);
    runLayout->addWidget(m_patchFrameLabel);
    runLayout->addWidget(m_patchFrame);
    runLayout->addWidget(m_progressBar, 1);

    m_logView = new QPlainTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(500);
    m_logView->setMaximumHeight(110);

    auto *rightLayout = new QVBoxLayout;
    rightLayout->addLayout(splitLayout);
    rightLayout->addWidget(m_table, 1);
    rightLayout->addLayout(rowLayout);
    rightLayout->addLayout(outputForm);
    rightLayout->addLayout(runLayout);
    rightLayout->addWidget(m_logView);
    auto *rightBox = new QGroupBox(QStringLiteral("分割片段"), this);
    rightBox->setLayout(rightLayout);

    // ---------- 整体布局 ----------
    auto *splitter = new QSplitter(this);
    splitter->addWidget(playerBox);
    splitter->addWidget(rightBox);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 4);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(splitter, 1);

    m_runner = new FFmpegRunner(this);

    // ---------- 信号 ----------
    connect(openButton, &QPushButton::clicked, this, &SplitWidget::openMediaFile);
    connect(avgButton, &QPushButton::clicked, this, &SplitWidget::doAverageSplit);
    connect(alignAsMinimaxButton, &QPushButton::clicked, this, &SplitWidget::doAlignAsMinimax);
    connect(markStartButton, &QPushButton::clicked, this, &SplitWidget::setStartFromPlayer);
    connect(markEndButton, &QPushButton::clicked, this, &SplitWidget::setEndFromPlayer);
    connect(splitHereButton, &QPushButton::clicked, this, &SplitWidget::splitAtPlayerPosition);
    connect(playSegButton, &QPushButton::clicked, this, &SplitWidget::playSelectedSegment);
    connect(removeButton, &QPushButton::clicked, this, &SplitWidget::removeSelectedRow);
    connect(upButton, &QPushButton::clicked, this, [this] { moveSelectedRow(-1); });
    connect(downButton, &QPushButton::clicked, this, [this] { moveSelectedRow(1); });
    connect(exportButton, &QPushButton::clicked, this, &SplitWidget::exportJson);
    connect(importButton, &QPushButton::clicked, this, &SplitWidget::importJson);
    connect(importSegOnlyButton, &QPushButton::clicked, this, &SplitWidget::importJsonSegOnly);
    connect(m_startButton, &QPushButton::clicked, this, &SplitWidget::startSplit);
    connect(m_cancelButton, &QPushButton::clicked, this, &SplitWidget::cancelSplit);
    connect(m_table, &QTableView::doubleClicked, this, &SplitWidget::onRowDoubleClicked);
    connect(browseDirButton, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择输出目录"), m_outputDirEdit->text());
        if (!dir.isEmpty())
            m_outputDirEdit->setText(dir);
    });

    connect(m_playButton, &QPushButton::clicked, this, [this] {
        if (m_player->playbackState() == QMediaPlayer::PlayingState)
            m_player->pause();
        else
            m_player->play();
    });
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState s) {
        m_playButton->setText(s == QMediaPlayer::PlayingState
                                  ? QStringLiteral("暂停") : QStringLiteral("播放"));
    });
    connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 ms) {
        m_positionSlider->setRange(0, int(ms));
    });
    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 ms) {
        if (!m_positionSlider->isSliderDown())
            m_positionSlider->setValue(int(ms));
        updateTransportTime(ms);
        m_waveform->setPosition(ms / 1000.0);
        m_waveformVideo->setPosition(ms / 1000.0);
        // 片段播放：到达终点自动暂停并停在终点
        if (m_playingSegment && ms >= m_segmentEndMs) {
            m_playingSegment = false;
            m_player->pause();
            m_player->setPosition(m_segmentEndMs);
        }
    });
    connect(m_waveform, &WaveformWidget::seekRequested, this, [this](double seconds) {
        m_playingSegment = false;
        m_player->setPosition(qint64(seconds * 1000.0));
    });
    connect(m_waveformVideo, &WaveformWidget::seekRequested, this, [this](double seconds) {
        m_playingSegment = false;
        m_player->setPosition(qint64(seconds * 1000.0));
    });
    connect(m_waveformToggle, &QCheckBox::toggled, this, [this](bool checked) {
        m_waveVideoBox->setVisible(checked);
    });
    // 波形缩放/平移滑块与波形视图双向联动
    connect(m_waveZoomSlider, &QSlider::valueChanged, this, [this](int v) {
        if (!m_updatingWaveView)
            m_waveform->setZoom(v / 10.0);
    });
    connect(m_wavePanSlider, &QSlider::valueChanged, this, [this](int v) {
        if (!m_updatingWaveView)
            m_waveform->setViewStartFraction(v / 1000.0);
    });
    connect(m_waveform, &WaveformWidget::viewChanged, this, [this] {
        m_updatingWaveView = true;
        m_waveZoomSlider->setValue(int(m_waveform->zoom() * 10.0));
        const double maxStart = m_waveform->maxViewStart();
        m_wavePanSlider->setEnabled(maxStart > 1e-9);
        m_wavePanSlider->setValue(maxStart > 1e-9 ? int(m_waveform->viewStart() / maxStart * 1000.0) : 0);
        m_updatingWaveView = false;
    });
    // 视频页紧凑波形的缩放/平移联动
    connect(m_waveVideoZoomSlider, &QSlider::valueChanged, this, [this](int v) {
        if (!m_updatingWaveVideoView)
            m_waveformVideo->setZoom(v / 10.0);
    });
    connect(m_waveVideoPanSlider, &QSlider::valueChanged, this, [this](int v) {
        if (!m_updatingWaveVideoView)
            m_waveformVideo->setViewStartFraction(v / 1000.0);
    });
    connect(m_waveformVideo, &WaveformWidget::viewChanged, this, [this] {
        m_updatingWaveVideoView = true;
        m_waveVideoZoomSlider->setValue(int(m_waveformVideo->zoom() * 10.0));
        const double maxStart = m_waveformVideo->maxViewStart();
        m_waveVideoPanSlider->setEnabled(maxStart > 1e-9);
        m_waveVideoPanSlider->setValue(maxStart > 1e-9 ? int(m_waveformVideo->viewStart() / maxStart * 1000.0) : 0);
        m_updatingWaveVideoView = false;
    });
    connect(m_waveformLoader, &WaveformLoader::finished, this,
            [this](const QVector<qint16> &mins, const QVector<qint16> &maxs, double duration) {
                m_waveform->setWaveform(mins, maxs, duration);
                m_waveformVideo->setWaveform(mins, maxs, duration);
            });
    connect(m_waveformLoader, &WaveformLoader::failed, this, [this](const QString &error) {
        m_waveform->clear();
        m_waveform->setMessage(QStringLiteral("波形生成失败：%1").arg(error));
        m_waveformVideo->clear();
        m_waveformVideo->setMessage(QStringLiteral("波形生成失败：%1").arg(error));
    });
    connect(m_positionSlider, &QSlider::sliderMoved, this, [this](int value) {
        m_playingSegment = false;
        m_player->setPosition(value);
    });

    // 打开视频后先播放再立刻暂停：让渲染管线输出首帧，
    // 否则界面黑屏，且暂停状态下拖动/跳转都不刷新画面
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (m_primeFirstFrame && status == QMediaPlayer::LoadedMedia)
                    m_player->play();
            });
    connect(m_player->videoSink(), &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &) {
                if (!m_primeFirstFrame)
                    return;
                m_primeFirstFrame = false;
                m_player->pause();
                m_player->setPosition(0);
            });
    connect(jumpTimeButton, &QPushButton::clicked, this, &SplitWidget::jumpToTimeInput);
    connect(m_jumpTimeEdit, &QLineEdit::returnPressed, this, &SplitWidget::jumpToTimeInput);
    connect(m_jumpFrameButton, &QPushButton::clicked, this, &SplitWidget::jumpToFrameInput);
    connect(m_jumpFrameSpin, &QSpinBox::editingFinished, this, [this] {
        if (m_jumpFrameSpin->hasFocus())
            jumpToFrameInput();
    });

    connect(m_runner, &FFmpegRunner::logLine, this, &SplitWidget::appendLog);
    connect(m_runner, &FFmpegRunner::progressChanged, this, [this](double f) {
        m_progressBar->setValue(int(f * 1000));
    });
    connect(m_runner, &FFmpegRunner::allFinished, this, [this](bool ok, const QString &msg) {
        setControlsEnabled(true);
        appendLog(msg);
        QMessageBox::information(this, QStringLiteral("分割"), msg);
    });

    setControlsEnabled(true);
}

void SplitWidget::setFfmpegPaths(const QString &ffmpeg, const QString &ffprobe)
{
    m_ffmpegPath = ffmpeg;
    m_ffprobePath = ffprobe;
    // 构造时路径尚未设置，开始按钮被禁用；路径就绪后重新启用
    if (!m_runner->isRunning())
        m_startButton->setEnabled(!m_ffmpegPath.isEmpty());
}

bool SplitWidget::loadMedia(const QString &filePath)
{
    if (m_ffprobePath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("未配置 ffmpeg，请先在“工具”菜单中设置"));
        return false;
    }
    const MediaInfo info = probeMedia(m_ffprobePath, filePath);
    if (!info.ok) {
        QMessageBox::warning(this, QStringLiteral("无法读取媒体"),
                             QStringLiteral("%1\n%2").arg(filePath, info.error));
        return false;
    }

    m_filePath = filePath;
    m_mediaInfo = info;
    m_model->setFps(info.fps);
    m_model->setMediaDuration(info.duration);
    m_model->setSegments({});

    QStringList desc;
    desc << QFileInfo(filePath).fileName();
    desc << QStringLiteral("时长 %1").arg(TimeUtils::formatTime(info.duration));
    if (info.hasVideo)
        desc << QStringLiteral("%1x%2 %3fps (%4)")
                    .arg(info.width).arg(info.height)
                    .arg(info.fps, 0, 'f', 3).arg(info.videoCodec);
    if (info.hasAudio)
        desc << QStringLiteral("音频 %1").arg(info.audioCodec);
    m_infoLabel->setText(desc.join(QStringLiteral(" | ")));

    m_player->setSource(QUrl::fromLocalFile(filePath));
    m_outputDirEdit->setText(QFileInfo(filePath).absolutePath());
    m_primeFirstFrame = info.hasVideo; // 视频文件：等待首帧渲染后自动暂停归零
    m_playingSegment = false;

    // 预览区：视频显示画面（含音频时下方可叠加波形），纯音频显示大波形
    if (info.hasVideo) {
        m_previewStack->setCurrentWidget(m_videoPage);
        if (info.hasAudio) {
            m_waveformToggle->setVisible(true);
            m_waveformVideo->clear();
            m_waveformVideo->setMessage(QStringLiteral("正在生成波形..."));
            m_waveVideoBox->setVisible(m_waveformToggle->isChecked());
            m_waveformLoader->load(m_ffmpegPath, filePath, info.duration);
        } else {
            m_waveformToggle->setVisible(false);
            m_waveVideoBox->hide();
            m_waveformLoader->cancel();
            m_waveformVideo->clear();
        }
        m_waveform->clear();
    } else {
        m_previewStack->setCurrentWidget(m_waveformPage);
        m_waveformToggle->setVisible(false);
        m_waveVideoBox->hide();
        m_waveform->clear();
        m_waveform->setMessage(QStringLiteral("正在生成波形..."));
        m_waveformLoader->load(m_ffmpegPath, filePath, info.duration);
    }

    // 帧跳转：仅视频（有帧率）可用，范围 0 ~ 总帧数
    const bool frameJumpOk = info.hasVideo && info.fps > 0;
    m_jumpFrameSpin->setEnabled(frameJumpOk);
    m_jumpFrameButton->setEnabled(frameJumpOk);
    if (frameJumpOk){
        m_jumpFrameSpin->setMaximum(int(TimeUtils::frameFromTime(info.duration, info.fps)));
        m_patchFrame->setVisible(info.hasVideo);
        m_patchFrameLabel->setVisible(info.hasVideo);
    }
    m_jumpFrameSpin->setValue(0);
    return true;
}

void SplitWidget::openMediaFile()
{
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开媒体文件"), QString(),
        QStringLiteral("媒体文件 (*.mp4 *.mkv *.avi *.mov *.ts *.flv *.wmv *.mp3 *.wav *.aac *.flac *.m4a *.ogg);;所有文件 (*)"));
    if (!file.isEmpty())
        loadMedia(file);
}

void SplitWidget::doAverageSplit()
{
    if (m_mediaInfo.duration <= 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先打开媒体文件"));
        return;
    }
    m_model->splitEvenly(m_mediaInfo.duration, m_countSpin->value());
}

void SplitWidget::doAlignAsMinimax()
{
    if (m_mediaInfo.duration <= 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先打开媒体文件"));
        return;
    }
    auto index = m_table->currentIndex();
    m_model->alignAsMinimax(m_mediaInfo.duration, m_keepContinuousCheck->isChecked());
    m_table->setCurrentIndex(index);
}

int SplitWidget::selectedRow() const
{
    const QModelIndexList sel = m_table->selectionModel()->selectedRows();
    return sel.isEmpty() ? -1 : sel.first().row();
}

void SplitWidget::setStartFromPlayer()
{
    const int row = selectedRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先在表格中选中一行"));
        return;
    }
    const QString value = TimeUtils::formatTime(m_player->position() / 1000.0);
    m_model->setData(m_model->index(row, SegmentModel::ColStart), value, Qt::EditRole);
    // 保持连续：上一段的终点同步为所设值
    if (m_keepContinuousCheck->isChecked() && row > 0)
        m_model->setData(m_model->index(row - 1, SegmentModel::ColEnd), value, Qt::EditRole);
}

void SplitWidget::setEndFromPlayer()
{
    const int row = selectedRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先在表格中选中一行"));
        return;
    }
    const QString value = TimeUtils::formatTime(m_player->position() / 1000.0);
    m_model->setData(m_model->index(row, SegmentModel::ColEnd), value, Qt::EditRole);
    // 保持连续：下一段的起点同步为所设值
    if (m_keepContinuousCheck->isChecked() && row + 1 < m_model->segments().size())
        m_model->setData(m_model->index(row + 1, SegmentModel::ColStart), value, Qt::EditRole);
}

void SplitWidget::splitAtPlayerPosition()
{
    const int row = selectedRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先在表格中选中一行"));
        return;
    }
    if (!m_model->splitRowAt(row, m_player->position() / 1000.0))
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("播放位置不在选中片段内部"));
}

void SplitWidget::removeSelectedRow()
{
    m_model->removeRowAt(selectedRow());
}

void SplitWidget::moveSelectedRow(int delta)
{
    const int row = selectedRow();
    if (m_model->moveRowAt(row, delta))
        m_table->selectRow(row + delta);
}

void SplitWidget::onRowDoubleClicked(const QModelIndex &index)
{
    if (index.row() < m_model->segments().size()) {
        m_playingSegment = false;
        m_player->setPosition(qint64(m_model->segments()[index.row()].start * 1000.0));
    }
}

void SplitWidget::playSelectedSegment()
{
    const int row = selectedRow();
    if (row < 0 || row >= m_model->segments().size()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在表格中选中要播放的片段"));
        return;
    }
    if (m_mediaInfo.duration <= 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先打开媒体文件"));
        return;
    }
    const Segment &seg = m_model->segments()[row];
    m_segmentEndMs = qMin(qint64(seg.end * 1000.0), m_player->duration());
    m_playingSegment = true;
    m_player->setPosition(qint64(seg.start * 1000.0));
    m_player->play(); // 到达片段终点时在 positionChanged 中自动暂停
}

void SplitWidget::jumpToTimeInput()
{
    if (m_mediaInfo.duration <= 0)
        return;
    bool ok = false;
    const double seconds = TimeUtils::parseTime(m_jumpTimeEdit->text(), &ok);
    if (!ok || seconds > m_mediaInfo.duration + 0.001) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("时间格式不正确或超出媒体时长"));
        return;
    }
    m_playingSegment = false;
    m_player->setPosition(qint64(seconds * 1000.0));
}

void SplitWidget::jumpToFrameInput()
{
    if (m_mediaInfo.fps <= 0)
        return;
    const qint64 frame = m_jumpFrameSpin->value();
    double seconds = TimeUtils::timeFromFrame(frame, m_mediaInfo.fps);
    seconds = qMin(seconds, m_mediaInfo.duration);
    m_playingSegment = false;
    m_player->setPosition(qint64(seconds * 1000.0));
}

void SplitWidget::exportJson()
{
    if (m_model->segments().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("片段列表为空"));
        return;
    }
    const QString file = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出片段 JSON"), QStringLiteral("segments.json"),
        QStringLiteral("JSON 文件 (*.json)"));
    if (file.isEmpty())
        return;

    QJsonArray arr;
    for (const Segment &seg : m_model->segments()) {
        QJsonObject obj;
        obj[QStringLiteral("start")] = TimeUtils::formatTime(seg.start);
        obj[QStringLiteral("end")] = TimeUtils::formatTime(seg.end);
        if (m_mediaInfo.fps > 0) {
            obj[QStringLiteral("start_frame")] = double(TimeUtils::frameFromTime(seg.start, m_mediaInfo.fps));
            obj[QStringLiteral("end_frame")] = double(TimeUtils::frameFromTime(seg.end, m_mediaInfo.fps));
        }
        arr.append(obj);
    }
    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("source")] = m_filePath;
    root[QStringLiteral("duration")] = m_mediaInfo.duration;
    if (m_mediaInfo.fps > 0)
        root[QStringLiteral("fps")] = m_mediaInfo.fps;
    root[QStringLiteral("segments")] = arr;

    QFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法写入 %1").arg(file));
        return;
    }
    f.write(QJsonDocument(root).toJson());
    appendLog(QStringLiteral("已导出 %1 个片段到 %2").arg(arr.size()).arg(file));
}

void SplitWidget::importJsonHelper(bool checkSource){
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入片段 JSON"), QString(),
        QStringLiteral("JSON 文件 (*.json)"));
    if (file.isEmpty())
        return;

    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法读取 %1").arg(file));
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("JSON 格式不正确"));
        return;
    }
    const QJsonObject root = doc.object();
    const QJsonArray arr = root.value(QStringLiteral("segments")).toArray();
    if (arr.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("JSON 中没有 segments"));
        return;
    }

    if(checkSource){
        // 如果 JSON 记录的源文件存在且与当前不同，自动加载
        const QString source = root.value(QStringLiteral("source")).toString();
        if (!source.isEmpty() && source != m_filePath && QFileInfo::exists(source)) {
            if (!loadMedia(source))
                return;
        }
    }

    if (m_mediaInfo.duration <= 0) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先打开与 JSON 对应的媒体文件"));
        return;
    }

    QVector<Segment> segments;
    segments.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        const QJsonObject obj = v.toObject();
        bool ok = false;
        Segment seg;
        seg.start = TimeUtils::parseTime(obj.value(QStringLiteral("start")).toVariant().toString(), &ok);
        if (ok){
            seg.end = TimeUtils::parseTime(obj.value(QStringLiteral("end")).toVariant().toString(), &ok);
        }
        if (!ok) {
            QMessageBox::warning(this, QStringLiteral("错误"),
                                 QStringLiteral("第 %1 个片段时间格式无法解析").arg(segments.size() + 1));
            return;
        }
        if(seg.start< m_mediaInfo.duration){
            if(seg.end > m_mediaInfo.duration){
                seg.end = m_mediaInfo.duration;
            }
            segments.append(seg);
        }

    }
    m_model->setSegments(segments);
    const QString err = m_model->validate();
    if (!err.isEmpty())
        QMessageBox::warning(this, QStringLiteral("警告"),
                             QStringLiteral("导入的片段存在问题：%1").arg(err));
    appendLog(QStringLiteral("已从 %1 导入 %2 个片段").arg(file).arg(segments.size()));
}

void SplitWidget::importJson()
{
    importJsonHelper(true);
}

void SplitWidget::importJsonSegOnly()
{
    importJsonHelper(false);
}

QString SplitWidget::outputFileFor(int index, int total, const QString &outputDir) const
{
    QString base = QFileInfo(m_filePath).completeBaseName();
    QString pattern = m_namePatternEdit->text().trimmed();
    if (pattern.isEmpty())
        pattern = QStringLiteral("part_%1");
    const int width = qMax(2, int(QString::number(total).size()));
    const QString number = QStringLiteral("%1").arg(index + 1, width, 10, QLatin1Char('0'));
    QString name = pattern;
    name.replace(QStringLiteral("%1"), number);
    if (name == pattern) // 没有占位符时自动追加序号
        name += QStringLiteral("_%1").arg(number);
    const QString ext = QFileInfo(m_filePath).suffix();
    return QDir(outputDir).filePath(QStringLiteral("%1_%2.%3").arg(base, name, ext));
}

QVector<FfmpegTask> SplitWidget::buildTasks(const QString &outputDir, bool reencode) const
{
    QVector<FfmpegTask> tasks;
    const QVector<Segment> &segments = m_model->segments();
    const QString suffix = QFileInfo(m_filePath).suffix().toLower();
    const bool audioOnly = !m_mediaInfo.hasVideo;
    // FLAC 容器在 stream copy 时会把源文件的 STREAMINFO.total_samples 原样写进输出头，
    // 导致切片的元数据显示为源文件全长（音频数据本身是对的）。因此输出到 flac 时
    // 即使选择了「无损直拷」也必须重编码（flac 重编码是无损的，无音质损失）。
    const bool flacOutput = (suffix == QLatin1String("flac"));
    double patchTime = 0;
    if(m_patchFrame->isVisible()){
        patchTime =  TimeUtils::timeFromFrame(m_patchFrame->value(), m_mediaInfo.fps);
    }
    for (int i = 0; i < segments.size(); ++i) {
        const Segment &seg = segments[i];
        const QString outFile = outputFileFor(i, segments.size(), outputDir);

        QStringList args;
        args << QStringLiteral("-y")
             << QStringLiteral("-ss") << QString::number(seg.start, 'f', 3)
             << QStringLiteral("-to") << QString::number(qMin(seg.end + patchTime, m_mediaInfo.duration), 'f', 3)
             << QStringLiteral("-i") << m_filePath;

        if (flacOutput) {
            // 重编码才会写出正确的 total_samples；-vn 排除封面图等视频流。
            // asetnsamples 不可省：精确 seek 后首帧往往只剩个位数样本（本例 6 帧），
            // 而 flac 编码器要求块大小 >= 16，会直接 invalid block size 失败；
            // 先用 asetnsamples 重新缓冲成固定的 4096 帧长即可，音频内容不变。
            args << QStringLiteral("-vn")
                 << QStringLiteral("-af") << QStringLiteral("asetnsamples=4096:p=0")
                 << QStringLiteral("-c:a") << QStringLiteral("flac");
        } else if (!reencode) {
            args << QStringLiteral("-map") << QStringLiteral("0")
                 << QStringLiteral("-c") << QStringLiteral("copy")
                 << QStringLiteral("-avoid_negative_ts") << QStringLiteral("make_zero");
        } else if (!audioOnly) {
            args << QStringLiteral("-map") << QStringLiteral("0:v?")
                 << QStringLiteral("-map") << QStringLiteral("0:a?")
                 << QStringLiteral("-c:v") << QStringLiteral("libx264")
                 << QStringLiteral("-preset") << QStringLiteral("veryfast")
                 << QStringLiteral("-crf") << QStringLiteral("18")
                 << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
                 << QStringLiteral("-c:a") << QStringLiteral("aac")
                 << QStringLiteral("-b:a") << QStringLiteral("192k");
        } else {
            args << QStringLiteral("-vn");
            if (suffix == QLatin1String("mp3"))
                args << QStringLiteral("-c:a") << QStringLiteral("libmp3lame")
                     << QStringLiteral("-q:a") << QStringLiteral("2");
            else if (suffix == QLatin1String("wav"))
                args << QStringLiteral("-c:a") << QStringLiteral("pcm_s16le");
            else if (suffix == QLatin1String("ogg"))
                args << QStringLiteral("-c:a") << QStringLiteral("libvorbis")
                     << QStringLiteral("-q:a") << QStringLiteral("5");
            else if (suffix == QLatin1String("opus"))
                args << QStringLiteral("-c:a") << QStringLiteral("libopus")
                     << QStringLiteral("-b:a") << QStringLiteral("128k");
            else if (suffix == QLatin1String("m4a") || suffix == QLatin1String("mp4"))
                args << QStringLiteral("-c:a") << QStringLiteral("aac")
                     << QStringLiteral("-b:a") << QStringLiteral("192k");
            // 其余后缀（aac/mka/webm 等）不额外指定编码器，让 ffmpeg 按容器自动选择。
            // 不要硬套 aac —— 把 aac 塞进不支持它的容器会直接 Encoding failed。
        }
        args << outFile;

        FfmpegTask task;
        task.label = QStringLiteral("片段 %1/%2 -> %3")
                         .arg(i + 1).arg(segments.size()).arg(QFileInfo(outFile).fileName());
        task.args = args;
        task.duration = seg.end - seg.start;
        tasks.append(task);
    }
    return tasks;
}

void SplitWidget::startSplit()
{
    if (m_ffmpegPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("未配置 ffmpeg，请先在“工具”菜单中设置"));
        return;
    }
    if (m_filePath.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先打开媒体文件"));
        return;
    }
    const QString err = m_model->validate();
    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法分割"), err);
        return;
    }
    const QString outputDir = m_outputDirEdit->text().trimmed();
    if (outputDir.isEmpty() || !QDir(outputDir).exists()) {
        QMessageBox::warning(this, QStringLiteral("无法分割"),
                             QStringLiteral("请选择有效的输出目录"));
        return;
    }
    const bool reencode = m_modeCombo->currentData().toBool();
    const QVector<FfmpegTask> tasks = buildTasks(outputDir, reencode);

    setControlsEnabled(false);
    m_progressBar->setValue(0);
    m_logView->clear();
    if (!reencode && QFileInfo(m_filePath).suffix().compare(QLatin1String("flac"), Qt::CaseInsensitive) == 0)
        m_logView->appendPlainText(QStringLiteral(
            "注意：FLAC 容器在直通拷贝时会继承源文件的 total_samples，切片时长会显示为原始全长。\n"
            "因此本次改用 flac 无损重编码（音质无损，速度略慢）以写入正确的时长。"));
    m_runner->start(m_ffmpegPath, tasks);
}

void SplitWidget::cancelSplit()
{
    m_runner->cancel();
}

void SplitWidget::setControlsEnabled(bool enabled)
{
    m_startButton->setEnabled(enabled && !m_ffmpegPath.isEmpty());
    m_cancelButton->setEnabled(!enabled);
    m_table->setEnabled(enabled);
}

void SplitWidget::appendLog(const QString &line)
{
    m_logView->appendPlainText(line);
}

void SplitWidget::updateTransportTime(qint64 positionMs)
{
    QString text = QStringLiteral("%1 / %2")
                       .arg(TimeUtils::formatTime(positionMs / 1000.0),
                            TimeUtils::formatTime(m_player->duration() / 1000.0));
    // 视频文件：附带当前帧位置（当前帧 / 总帧数）
    if (m_mediaInfo.hasVideo && m_mediaInfo.fps > 0) {
        const qint64 frame = TimeUtils::frameFromTime(positionMs / 1000.0, m_mediaInfo.fps);
        const qint64 total = TimeUtils::frameFromTime(m_mediaInfo.duration, m_mediaInfo.fps);
        text += QStringLiteral("  |  帧 %1 / %2").arg(frame).arg(total);
    }
    m_timeLabel->setText(text);
}
