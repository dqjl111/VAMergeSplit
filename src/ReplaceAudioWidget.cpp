#include "ReplaceAudioWidget.h"

#include "FFmpegRunner.h"
#include "MediaInfo.h"
#include "TimeUtils.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

// 按输出容器选择音频编码器（与合并页保持一致）。
// 不要对所有容器硬套 aac —— flac/ogg/opus 这类容器会拒绝 aac 写入。
QStringList audioEncoderArgs(const QString &suffix)
{
    if (suffix == QLatin1String("mp3"))
        return {QStringLiteral("-c:a"), QStringLiteral("libmp3lame"),
                QStringLiteral("-q:a"), QStringLiteral("2")};
    if (suffix == QLatin1String("wav"))
        return {QStringLiteral("-c:a"), QStringLiteral("pcm_s16le")};
    if (suffix == QLatin1String("flac"))
        return {QStringLiteral("-c:a"), QStringLiteral("flac")};
    if (suffix == QLatin1String("ogg") || suffix == QLatin1String("oga"))
        return {QStringLiteral("-c:a"), QStringLiteral("libvorbis"),
                QStringLiteral("-q:a"), QStringLiteral("5")};
    if (suffix == QLatin1String("opus") || suffix == QLatin1String("webm"))
        return {QStringLiteral("-c:a"), QStringLiteral("libopus"),
                QStringLiteral("-b:a"), QStringLiteral("128k")};
    // 其余容器（mp4/mov/mkv/mka/aac 等）用 aac
    return {QStringLiteral("-c:a"), QStringLiteral("aac"),
            QStringLiteral("-b:a"), QStringLiteral("192k")};
}

} // namespace

ReplaceAudioWidget::ReplaceAudioWidget(QWidget *parent)
    : QWidget(parent)
{
    m_videoEdit = new QLineEdit(this);
    auto *videoButton = new QPushButton(QStringLiteral("浏览..."), this);
    auto *videoLayout = new QHBoxLayout;
    videoLayout->addWidget(m_videoEdit, 1);
    videoLayout->addWidget(videoButton);

    m_audioEdit = new QLineEdit(this);
    auto *audioButton = new QPushButton(QStringLiteral("浏览..."), this);
    auto *audioLayout = new QHBoxLayout;
    audioLayout->addWidget(m_audioEdit, 1);
    audioLayout->addWidget(audioButton);

    m_outputEdit = new QLineEdit(this);
    auto *outputButton = new QPushButton(QStringLiteral("浏览..."), this);
    auto *outputLayout = new QHBoxLayout;
    outputLayout->addWidget(m_outputEdit, 1);
    outputLayout->addWidget(outputButton);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QStringLiteral("无损复制（音频 stream copy，要求编码被输出容器支持）"), false);
    m_modeCombo->addItem(QStringLiteral("重编码音频（兼容所有容器）"), true);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("视频文件:"), videoLayout);
    form->addRow(QStringLiteral("音频文件:"), audioLayout);
    form->addRow(QStringLiteral("输出文件:"), outputLayout);
    form->addRow(QStringLiteral("音频模式:"), m_modeCombo);

    m_infoLabel = new QLabel(QStringLiteral("选择视频与音频文件后，将使用新的音频替换视频原有音轨（视频画面无损直拷）。"), this);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setStyleSheet(QStringLiteral("color: gray;"));

    m_startButton = new QPushButton(QStringLiteral("开始替换"), this);
    m_startButton->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_cancelButton = new QPushButton(QStringLiteral("取消"), this);
    m_cancelButton->setEnabled(false);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 1000);
    auto *runLayout = new QHBoxLayout;
    runLayout->addWidget(m_startButton);
    runLayout->addWidget(m_cancelButton);
    runLayout->addWidget(m_progressBar, 1);

    m_logView = new QPlainTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(500);
    m_logView->setMaximumHeight(110);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_infoLabel);
    layout->addLayout(runLayout);
    layout->addWidget(m_logView);
    layout->addStretch(1);

    m_runner = new FFmpegRunner(this);

    connect(videoButton, &QPushButton::clicked, this, &ReplaceAudioWidget::browseVideo);
    connect(audioButton, &QPushButton::clicked, this, &ReplaceAudioWidget::browseAudio);
    connect(outputButton, &QPushButton::clicked, this, &ReplaceAudioWidget::browseOutput);
    connect(m_startButton, &QPushButton::clicked, this, &ReplaceAudioWidget::startReplace);
    connect(m_cancelButton, &QPushButton::clicked, this, &ReplaceAudioWidget::cancelReplace);

    connect(m_runner, &FFmpegRunner::logLine, this, &ReplaceAudioWidget::appendLog);
    connect(m_runner, &FFmpegRunner::progressChanged, this, [this](double f) {
        m_progressBar->setValue(int(f * 1000));
    });
    connect(m_runner, &FFmpegRunner::allFinished, this, [this](bool ok, const QString &msg) {
        m_startButton->setEnabled(true);
        m_cancelButton->setEnabled(false);
        appendLog(msg);
        QMessageBox::information(this, QStringLiteral("音频替换"), msg);
    });
}

void ReplaceAudioWidget::setFfmpegPaths(const QString &ffmpeg, const QString &ffprobe)
{
    m_ffmpegPath = ffmpeg;
    m_ffprobePath = ffprobe;
}

void ReplaceAudioWidget::browseVideo()
{
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择视频文件"), m_videoEdit->text(),
        QStringLiteral("视频文件 (*.mp4 *.mkv *.avi *.mov *.ts *.flv *.wmv);;所有文件 (*)"));
    if (file.isEmpty())
        return;
    m_videoEdit->setText(file);

    if (!m_ffprobePath.isEmpty()) {
        const MediaInfo info = probeMedia(m_ffprobePath, file);
        if (info.ok && !info.hasVideo)
            QMessageBox::warning(this, QStringLiteral("提示"),
                                 QStringLiteral("该文件不包含视频流，无法作为替换对象。"));
    }

    // 自动填充输出文件名
    if (m_outputEdit->text().isEmpty()) {
        const QFileInfo vi(file);
        m_outputEdit->setText(vi.absolutePath() + QLatin1Char('/')
                              + vi.completeBaseName() + QStringLiteral("_新音频.mp4"));
    }
}

void ReplaceAudioWidget::browseAudio()
{
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择音频文件"), m_audioEdit->text(),
        QStringLiteral("音频文件 (*.mp3 *.wav *.aac *.flac *.m4a *.ogg *.opus *.mka);;所有文件 (*)"));
    if (file.isEmpty())
        return;
    m_audioEdit->setText(file);

    if (!m_ffprobePath.isEmpty()) {
        const MediaInfo info = probeMedia(m_ffprobePath, file);
        if (info.ok && !info.hasAudio)
            QMessageBox::warning(this, QStringLiteral("提示"),
                                 QStringLiteral("该文件不包含音频流，无法作为替换音轨。"));
    }
}

void ReplaceAudioWidget::browseOutput()
{
    const QString file = QFileDialog::getSaveFileName(
        this, QStringLiteral("选择输出文件"), m_outputEdit->text(),
        QStringLiteral("视频 (*.mp4 *.mkv *.mov *.ts);;所有文件 (*)"));
    if (!file.isEmpty())
        m_outputEdit->setText(file);
}

void ReplaceAudioWidget::startReplace()
{
    if (m_ffmpegPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("未配置 ffmpeg，请先在“工具”菜单中设置"));
        return;
    }
    const QString video = m_videoEdit->text().trimmed();
    const QString audio = m_audioEdit->text().trimmed();
    const QString outFile = m_outputEdit->text().trimmed();
    if (video.isEmpty() || !QFile::exists(video)) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("请选择有效的视频文件"));
        return;
    }
    if (audio.isEmpty() || !QFile::exists(audio)) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("请选择有效的音频文件"));
        return;
    }
    if (outFile.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("请指定输出文件"));
        return;
    }

    // 探测时长用于进度估算（缺失则用 0，运行器会退化为按任务计数）
    double duration = 0;
    if (!m_ffprobePath.isEmpty()) {
        const MediaInfo info = probeMedia(m_ffprobePath, video);
        if (info.ok)
            duration = qMax(info.duration, probeMedia(m_ffprobePath, audio).duration);
    }

    const bool reencode = m_modeCombo->currentData().toBool();
    const QString suffix = QFileInfo(outFile).suffix().toLower();

    QStringList args;
    args << QStringLiteral("-y")
         << QStringLiteral("-i") << video
         << QStringLiteral("-i") << audio
         << QStringLiteral("-map") << QStringLiteral("0:v:0")
         << QStringLiteral("-map") << QStringLiteral("1:a:0")
         << QStringLiteral("-c:v") << QStringLiteral("copy");
    if (reencode)
        args << audioEncoderArgs(suffix);
    else
        args << QStringLiteral("-c:a") << QStringLiteral("copy");
    args << outFile;

    FfmpegTask task;
    task.label = QStringLiteral("替换音频 -> %1").arg(QFileInfo(outFile).fileName());
    task.args = args;
    task.duration = duration;

    m_startButton->setEnabled(false);
    m_cancelButton->setEnabled(true);
    m_progressBar->setValue(0);
    m_logView->clear();
    m_runner->start(m_ffmpegPath, {task});
}

void ReplaceAudioWidget::cancelReplace()
{
    m_runner->cancel();
}

void ReplaceAudioWidget::appendLog(const QString &line)
{
    m_logView->appendPlainText(line);
}
