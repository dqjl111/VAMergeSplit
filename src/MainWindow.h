#pragma once

#include <QMainWindow>

class QTabWidget;
class SplitWidget;
class MergeWidget;
class ReplaceAudioWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void detectFfmpeg();
    void applyFfmpegPaths();
    void chooseFfmpegDir();

    QString m_ffmpegPath;
    QString m_ffprobePath;

    QTabWidget *m_tabs = nullptr;
    SplitWidget *m_splitWidget = nullptr;
    MergeWidget *m_mergeWidget = nullptr;
    ReplaceAudioWidget *m_replaceAudioWidget = nullptr;
};
