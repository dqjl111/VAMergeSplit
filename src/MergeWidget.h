#pragma once

#include <QList>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

class FFmpegRunner;

// 合并页：表格（文件名 / 合并起止 / 合并后起止）+ 裁剪 + 导入分割/合并数据 + concat 合并
class MergeWidget : public QWidget {
    Q_OBJECT
public:
    explicit MergeWidget(QWidget *parent = nullptr);

    void setFfmpegPaths(const QString &ffmpeg, const QString &ffprobe);

private slots:
    void addFiles();
    void removeSelected();
    void sortByName();
    void moveSelected(int delta);
    void clearTrim();
    void importSplitData();
    void exportSplitData();
    void importMergeData();
    void exportMergeData();
    void startMerge();
    void cancelMerge();

private:
    struct RowData {
        QString path;
        double duration = 0; // 文件完整时长（秒）
        double start = 0;    // 合并起（裁剪起点）
        double end = -1;     // 合并止（<=0 表示文件尾）
        bool hasVideo = false;
        bool hasAudio = false;
    };

    enum Column {
        ColName = 0,   // 文件名
        ColFileDuration, // 原始时长（文件完整时长，不随起止变化）
        ColStart,      // 合并起（可编辑）
        ColEnd,        // 合并止（可编辑）
        ColDuration,   // 有效时长（自动计算）
        ColOutStart,   // 合并后起（自动计算）
        ColOutEnd      // 合并后止（自动计算）
    };

    void appendLog(const QString &line);
    RowData rowData(int row) const;
    void appendRow(const RowData &d);
    void rebuildTable(const QList<RowData> &rows, int selectRow = -1);
    void refreshTrimCells(int row, const RowData &d);
    double effectiveEnd(const RowData &d) const;
    double effectiveDuration(const RowData &d) const;
    void recomputeTimeline();
    void onItemChanged(QTableWidgetItem *item);

    QString m_ffmpegPath;
    QString m_ffprobePath;

    QTableWidget *m_table = nullptr;
    QLabel *m_totalLabel = nullptr;
    QLineEdit *m_outputEdit = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPlainTextEdit *m_logView = nullptr;
    bool m_updatingTable = false;

    FFmpegRunner *m_runner = nullptr;
};
