#include "MainWindow.h"

#include "MergeWidget.h"
#include "SplitWidget.h"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("VAMergeSplit - 音视频分割合并工具"));
    resize(1280, 780);

    m_tabs = new QTabWidget(this);
    m_splitWidget = new SplitWidget(this);
    m_mergeWidget = new MergeWidget(this);
    m_tabs->addTab(m_splitWidget, QStringLiteral("分割"));
    m_tabs->addTab(m_mergeWidget, QStringLiteral("合并"));
    setCentralWidget(m_tabs);

    auto *toolsMenu = menuBar()->addMenu(QStringLiteral("工具(&T)"));
    auto *setFfmpegAction = toolsMenu->addAction(QStringLiteral("设置 FFmpeg 目录..."));
    connect(setFfmpegAction, &QAction::triggered, this, &MainWindow::chooseFfmpegDir);

    detectFfmpeg();
    applyFfmpegPaths();
}

void MainWindow::detectFfmpeg()
{
    QSettings settings;
    const QString saved = settings.value(QStringLiteral("ffmpegDir")).toString();

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        saved,
        appDir + QStringLiteral("/ffmpeg-9.0.2-full_build/bin"),
        appDir + QStringLiteral("/../ffmpeg-9.0.2-full_build/bin"),
        appDir + QStringLiteral("/../../ffmpeg-9.0.2-full_build/bin"),
        QString() // PATH
    };

    for (const QString &dir : candidates) {
        QString ffmpeg, ffprobe;
        if (dir.isEmpty()) {
            ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
            ffprobe = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
        } else {
            const QDir d(dir);
            const QString f = d.filePath(QStringLiteral("ffmpeg.exe"));
            const QString p = d.filePath(QStringLiteral("ffprobe.exe"));
            if (QFileInfo::exists(f) && QFileInfo::exists(p)) {
                ffmpeg = f;
                ffprobe = p;
            }
        }
        if (!ffmpeg.isEmpty() && !ffprobe.isEmpty()) {
            m_ffmpegPath = QFileInfo(ffmpeg).absoluteFilePath();
            m_ffprobePath = QFileInfo(ffprobe).absoluteFilePath();
            return;
        }
    }
}

void MainWindow::applyFfmpegPaths()
{
    if (m_ffmpegPath.isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("未找到 ffmpeg：请在“工具”菜单中手动选择 ffmpeg 所在目录"));
    } else {
        statusBar()->showMessage(QStringLiteral("ffmpeg: %1").arg(m_ffmpegPath));
    }
    m_splitWidget->setFfmpegPaths(m_ffmpegPath, m_ffprobePath);
    m_mergeWidget->setFfmpegPaths(m_ffmpegPath, m_ffprobePath);
}

void MainWindow::chooseFfmpegDir()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择包含 ffmpeg.exe / ffprobe.exe 的目录"),
        m_ffmpegPath.isEmpty() ? QString() : QFileInfo(m_ffmpegPath).absolutePath());
    if (dir.isEmpty())
        return;

    const QDir d(dir);
    if (!QFileInfo::exists(d.filePath(QStringLiteral("ffmpeg.exe")))
        || !QFileInfo::exists(d.filePath(QStringLiteral("ffprobe.exe")))) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("所选目录中没有 ffmpeg.exe 或 ffprobe.exe"));
        return;
    }
    QSettings().setValue(QStringLiteral("ffmpegDir"), dir);
    m_ffmpegPath = d.filePath(QStringLiteral("ffmpeg.exe"));
    m_ffprobePath = d.filePath(QStringLiteral("ffprobe.exe"));
    applyFfmpegPaths();
}
