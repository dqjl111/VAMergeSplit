#include "MergeWidget.h"

#include "FFmpegRunner.h"
#include "MediaInfo.h"
#include "TimeUtils.h"

#include <QCollator>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

// 按输出容器选择音频编码器。
// 关键：不要对所有容器硬套 aac —— 把 aac 塞进 flac/ogg/opus 这类容器，
// muxer 会直接拒绝写头（flac: "Invalid audio stream. Exactly one FLAC audio stream is required."），
// 整个任务 Conversion failed。
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

// 纯音频容器：即使输入带视频也不写视频流
bool isAudioOnlyContainer(const QString &suffix)
{
    return suffix == QLatin1String("mp3") || suffix == QLatin1String("wav")
        || suffix == QLatin1String("flac") || suffix == QLatin1String("m4a")
        || suffix == QLatin1String("aac") || suffix == QLatin1String("ogg")
        || suffix == QLatin1String("oga") || suffix == QLatin1String("opus")
        || suffix == QLatin1String("mka");
}

} // namespace

MergeWidget::MergeWidget(QWidget *parent)
    : QWidget(parent)
{
    // ---------- 文件表格 ----------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("文件名"), QStringLiteral("原始时长"),
        QStringLiteral("合并起"), QStringLiteral("合并止"),
        QStringLiteral("有效时长"), QStringLiteral("合并后起"), QStringLiteral("合并后止")});
    m_table->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked |
                             QAbstractItemView::EditKeyPressed |
                             QAbstractItemView::AnyKeyPressed);

    auto *addButton = new QPushButton(QStringLiteral("添加文件..."), this);
    auto *removeButton = new QPushButton(QStringLiteral("移除选中"), this);
    auto *upButton = new QPushButton(QStringLiteral("上移"), this);
    auto *downButton = new QPushButton(QStringLiteral("下移"), this);
    auto *sortButton = new QPushButton(QStringLiteral("按文件名排序"), this);
    auto *clearTrimButton = new QPushButton(QStringLiteral("清除选中裁剪"), this);
    auto *row1Layout = new QHBoxLayout;
    row1Layout->addWidget(addButton);
    row1Layout->addWidget(removeButton);
    row1Layout->addWidget(upButton);
    row1Layout->addWidget(downButton);
    row1Layout->addWidget(sortButton);
    row1Layout->addStretch(1);
    row1Layout->addWidget(clearTrimButton);

    auto *importSplitButton = new QPushButton(QStringLiteral("导入分割数据..."), this);
    auto *exportSplitButton = new QPushButton(QStringLiteral("导出分割数据..."), this);
    auto *importButton = new QPushButton(QStringLiteral("导入合并数据..."), this);
    auto *exportButton = new QPushButton(QStringLiteral("导出合并数据..."), this);
    auto *row2Layout = new QHBoxLayout;
    row2Layout->addStretch(1);
    row2Layout->addWidget(importSplitButton);
    row2Layout->addWidget(exportSplitButton);
    row2Layout->addWidget(importButton);
    row2Layout->addWidget(exportButton);

    m_totalLabel = new QLabel(QStringLiteral("共 0 个文件"), this);
    auto *hintLabel = new QLabel(
        QStringLiteral("说明：双击“合并起/合并止”单元格可编辑该段参与合并的时间范围"
                       "（支持 秒、MM:SS、HH:MM:SS.mmm）；“合并后起/止”为该段在输出文件中的时间位置。"
                       "“导入分割数据”可读取分割页导出的片段 JSON，自动按原始有效时间段设置各段裁剪"
                       "（无损直拷分割会按源文件关键帧计算被补齐的偏移）。"
                       "设置裁剪后合并将使用重编码以保证精确。"), this);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QStringLiteral("color: gray;"));

    // ---------- 输出 ----------
    m_outputEdit = new QLineEdit(this);
    auto *browseButton = new QPushButton(QStringLiteral("浏览..."), this);
    auto *outLayout = new QHBoxLayout;
    outLayout->addWidget(m_outputEdit, 1);
    outLayout->addWidget(browseButton);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QStringLiteral("无损直拷（快，要求编码一致，不支持裁剪）"), false);
    m_modeCombo->addItem(QStringLiteral("重编码（慢，兼容不同来源，支持精确裁剪）"), true);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("输出文件:"), outLayout);
    form->addRow(QStringLiteral("输出模式:"), m_modeCombo);

    m_startButton = new QPushButton(QStringLiteral("开始合并"), this);
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
    layout->addLayout(row1Layout);
    layout->addLayout(row2Layout);
    layout->addWidget(m_table, 1);
    layout->addWidget(m_totalLabel);
    layout->addWidget(hintLabel);
    layout->addLayout(form);
    layout->addLayout(runLayout);
    layout->addWidget(m_logView);

    m_runner = new FFmpegRunner(this);

    // ---------- 信号 ----------
    connect(addButton, &QPushButton::clicked, this, &MergeWidget::addFiles);
    connect(removeButton, &QPushButton::clicked, this, &MergeWidget::removeSelected);
    connect(upButton, &QPushButton::clicked, this, [this] { moveSelected(-1); });
    connect(downButton, &QPushButton::clicked, this, [this] { moveSelected(1); });
    connect(sortButton, &QPushButton::clicked, this, &MergeWidget::sortByName);
    connect(clearTrimButton, &QPushButton::clicked, this, &MergeWidget::clearTrim);
    connect(importSplitButton, &QPushButton::clicked, this, &MergeWidget::importSplitData);
    connect(exportSplitButton, &QPushButton::clicked, this, &MergeWidget::exportSplitData);
    connect(importButton, &QPushButton::clicked, this, &MergeWidget::importMergeData);
    connect(exportButton, &QPushButton::clicked, this, &MergeWidget::exportMergeData);
    connect(m_startButton, &QPushButton::clicked, this, &MergeWidget::startMerge);
    connect(m_cancelButton, &QPushButton::clicked, this, &MergeWidget::cancelMerge);
    connect(m_table, &QTableWidget::itemChanged, this, &MergeWidget::onItemChanged);
    connect(browseButton, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getSaveFileName(
            this, QStringLiteral("选择输出文件"), m_outputEdit->text(),
            QStringLiteral("视频 (*.mp4 *.mkv *.mov *.ts);;音频 (*.mp3 *.m4a *.wav *.aac *.flac *.ogg *.opus *.mka)"));
        if (!file.isEmpty())
            m_outputEdit->setText(file);
    });

    connect(m_runner, &FFmpegRunner::logLine, this, &MergeWidget::appendLog);
    connect(m_runner, &FFmpegRunner::progressChanged, this, [this](double f) {
        m_progressBar->setValue(int(f * 1000));
    });
    connect(m_runner, &FFmpegRunner::allFinished, this, [this](bool ok, const QString &msg) {
        m_startButton->setEnabled(true);
        m_cancelButton->setEnabled(false);
        appendLog(msg);
        QMessageBox::information(this, QStringLiteral("合并"), msg);
    });
}

void MergeWidget::setFfmpegPaths(const QString &ffmpeg, const QString &ffprobe)
{
    m_ffmpegPath = ffmpeg;
    m_ffprobePath = ffprobe;
}

// ---------- 表格数据 ----------

double MergeWidget::effectiveEnd(const RowData &d) const
{
    return (d.end <= 0 || d.end > d.duration) ? d.duration : d.end;
}

double MergeWidget::effectiveDuration(const RowData &d) const
{
    return qMax(0.0, effectiveEnd(d) - d.start);
}

MergeWidget::RowData MergeWidget::rowData(int row) const
{
    RowData d;
    QTableWidgetItem *it = m_table->item(row, ColName);
    if (!it)
        return d;
    d.path = it->data(Qt::UserRole).toString();
    d.duration = it->data(Qt::UserRole + 1).toDouble();
    d.start = it->data(Qt::UserRole + 2).toDouble();
    d.end = it->data(Qt::UserRole + 3).toDouble();
    d.hasVideo = it->data(Qt::UserRole + 4).toBool();
    d.hasAudio = it->data(Qt::UserRole + 5).toBool();
    return d;
}

void MergeWidget::appendRow(const RowData &d)
{
    // setItem 会触发 itemChanged，整个填充过程都要屏蔽
    m_updatingTable = true;
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    const Qt::ItemFlags roFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    auto *name = new QTableWidgetItem(QFileInfo(d.path).fileName());
    name->setFlags(roFlags);
    name->setToolTip(d.path);
    name->setData(Qt::UserRole, d.path);
    name->setData(Qt::UserRole + 1, d.duration);
    name->setData(Qt::UserRole + 2, d.start);
    name->setData(Qt::UserRole + 3, d.end);
    name->setData(Qt::UserRole + 4, d.hasVideo);
    name->setData(Qt::UserRole + 5, d.hasAudio);
    m_table->setItem(row, ColName, name);

    auto *fileDur = new QTableWidgetItem(
        d.duration > 0 ? TimeUtils::formatTime(d.duration) : QStringLiteral("-"));
    fileDur->setFlags(roFlags);
    m_table->setItem(row, ColFileDuration, fileDur);

    auto *start = new QTableWidgetItem;
    auto *end = new QTableWidgetItem;
    m_table->setItem(row, ColStart, start);
    m_table->setItem(row, ColEnd, end);

    auto *dur = new QTableWidgetItem;
    dur->setFlags(roFlags);
    m_table->setItem(row, ColDuration, dur);

    auto *outStart = new QTableWidgetItem;
    outStart->setFlags(roFlags);
    m_table->setItem(row, ColOutStart, outStart);
    auto *outEnd = new QTableWidgetItem;
    outEnd->setFlags(roFlags);
    m_table->setItem(row, ColOutEnd, outEnd);

    m_table->item(row, ColStart)->setText(TimeUtils::formatTime(d.start));
    m_table->item(row, ColEnd)->setText(TimeUtils::formatTime(effectiveEnd(d)));
    m_updatingTable = false;
}

void MergeWidget::refreshTrimCells(int row, const RowData &d)
{
    QTableWidgetItem *startItem = m_table->item(row, ColStart);
    QTableWidgetItem *endItem = m_table->item(row, ColEnd);
    if (!startItem || !endItem)
        return;
    m_updatingTable = true;
    startItem->setText(TimeUtils::formatTime(d.start));
    endItem->setText(TimeUtils::formatTime(effectiveEnd(d)));
    m_updatingTable = false;
}

void MergeWidget::rebuildTable(const QList<RowData> &rows, int selectRow)
{
    m_updatingTable = true;
    m_table->setRowCount(0);
    m_updatingTable = false;
    for (const RowData &d : rows)
        appendRow(d);
    if (selectRow >= 0 && selectRow < m_table->rowCount())
        m_table->setCurrentCell(selectRow, ColName);
    recomputeTimeline();
}

void MergeWidget::recomputeTimeline()
{
    m_updatingTable = true;
    double cum = 0;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        const RowData d = rowData(r);
        const double eff = effectiveDuration(d);
        if (QTableWidgetItem *it = m_table->item(r, ColDuration))
            it->setText(TimeUtils::formatTime(eff));
        if (QTableWidgetItem *it = m_table->item(r, ColOutStart))
            it->setText(TimeUtils::formatTime(cum));
        if (QTableWidgetItem *it = m_table->item(r, ColOutEnd))
            it->setText(TimeUtils::formatTime(cum + eff));
        cum += eff;
    }
    m_updatingTable = false;
    m_totalLabel->setText(QStringLiteral("共 %1 个文件，有效总时长约 %2")
                              .arg(m_table->rowCount())
                              .arg(TimeUtils::formatTime(cum)));
}

void MergeWidget::onItemChanged(QTableWidgetItem *item)
{
    if (m_updatingTable || !item)
        return;
    const int col = item->column();
    if (col != ColStart && col != ColEnd)
        return;

    const int row = item->row();
    RowData d = rowData(row);
    double value = (col == ColStart) ? 0.0 : d.duration;
    const QString text = item->text().trimmed();
    if (!text.isEmpty()) {
        bool ok = false;
        value = TimeUtils::parseTime(text, &ok);
        if (!ok) {
            QMessageBox::warning(this, QStringLiteral("错误"),
                                 QStringLiteral("无法解析时间：%1").arg(text));
            refreshTrimCells(row, d);
            return;
        }
    }

    double start = (col == ColStart) ? value : d.start;
    double end = (col == ColEnd) ? value : effectiveEnd(d);
    if (end > d.duration && d.duration > 0)
        end = d.duration; // 超出文件尾自动截断
    if (start < 0 || start >= end) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("起止时间无效，需满足 0 ≤ 合并起 < 合并止"));
        refreshTrimCells(row, d);
        return;
    }

    d.start = start;
    d.end = end;
    QTableWidgetItem *nameItem = m_table->item(row, ColName);
    nameItem->setData(Qt::UserRole + 2, d.start);
    nameItem->setData(Qt::UserRole + 3, d.end);
    refreshTrimCells(row, d);
    recomputeTimeline();
}

// ---------- 列表操作 ----------

void MergeWidget::addFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("添加媒体文件"), QString(),
        QStringLiteral("媒体文件 (*.mp4 *.mkv *.avi *.mov *.ts *.flv *.wmv *.mp3 *.wav *.aac *.flac *.m4a *.ogg);;所有文件 (*)"));
    if (files.isEmpty())
        return;

    // 批量探测媒体信息，显示进度避免界面假死
    QProgressDialog progress(QStringLiteral("正在读取媒体信息..."),
                             QStringLiteral("取消"), 0, files.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setWindowTitle(QStringLiteral("添加文件"));
    progress.setMinimumDuration(300);
    progress.setValue(0);

    int done = 0;
    for (const QString &file : files) {
        progress.setLabelText(QFileInfo(file).fileName());
        if (progress.wasCanceled())
            break;
        RowData d;
        d.path = file;
        if (!m_ffprobePath.isEmpty()) {
            const MediaInfo info = probeMedia(m_ffprobePath, file);
            if (info.ok) {
                d.duration = info.duration;
                d.hasVideo = info.hasVideo;
                d.hasAudio = info.hasAudio;
            }
        }
        appendRow(d);
        progress.setValue(++done);
    }
    progress.close();
    recomputeTimeline();
    if (m_outputEdit->text().isEmpty() && !files.isEmpty())
        m_outputEdit->setText(QDir(QFileInfo(files.first()).absolutePath())
                                  .filePath(QStringLiteral("merged.%1")
                                                .arg(QFileInfo(files.first()).suffix())));
}

void MergeWidget::removeSelected()
{
    QList<int> rows;
    for (const QModelIndex &idx : m_table->selectionModel()->selectedRows())
        rows.append(idx.row());
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    m_updatingTable = true;
    for (int r : rows)
        m_table->removeRow(r);
    m_updatingTable = false;
    recomputeTimeline();
}

void MergeWidget::moveSelected(int delta)
{
    const int row = m_table->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= m_table->rowCount())
        return;
    QList<RowData> rows;
    for (int r = 0; r < m_table->rowCount(); ++r)
        rows.append(rowData(r));
    rows.swapItemsAt(row, target);
    rebuildTable(rows, target);
}

void MergeWidget::sortByName()
{
    // 自然排序：数字按数值比较（part2 < part10），忽略大小写
    static QCollator collator = [] {
        QCollator c;
        c.setNumericMode(true);
        c.setCaseSensitivity(Qt::CaseInsensitive);
        return c;
    }();

    QList<RowData> rows;
    for (int r = 0; r < m_table->rowCount(); ++r)
        rows.append(rowData(r));
    std::sort(rows.begin(), rows.end(), [](const RowData &a, const RowData &b) {
        return collator.compare(QFileInfo(a.path).fileName(),
                                QFileInfo(b.path).fileName()) < 0;
    });
    rebuildTable(rows);
}

void MergeWidget::clearTrim()
{
    QList<int> rows;
    for (const QModelIndex &idx : m_table->selectionModel()->selectedRows())
        rows.append(idx.row());
    if (rows.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选中要清除裁剪的行"));
        return;
    }
    for (int r : rows) {
        RowData d = rowData(r);
        d.start = 0;
        d.end = -1;
        QTableWidgetItem *nameItem = m_table->item(r, ColName);
        nameItem->setData(Qt::UserRole + 2, d.start);
        nameItem->setData(Qt::UserRole + 3, d.end);
        refreshTrimCells(r, d);
    }
    recomputeTimeline();
}

// ---------- 导入 / 导出 ----------

void MergeWidget::importSplitData()
{
    if (m_table->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先添加分割后的片段文件"));
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入分割数据"), QString(),
        QStringLiteral("JSON 文件 (*.json);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法打开 %1").arg(path));
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    const QJsonObject root = doc.object();
    const QJsonArray arr = root.value(QStringLiteral("segments")).toArray();
    if (arr.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("JSON 中没有 segments（不是分割页导出的片段数据）"));
        return;
    }

    // 解析各片段起止时间（相对于源文件）
    QVector<double> segStart, segEnd;
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        bool okS = false, okE = false;
        const double s = TimeUtils::parseTime(o.value(QStringLiteral("start")).toVariant().toString(), &okS);
        const double e = TimeUtils::parseTime(o.value(QStringLiteral("end")).toVariant().toString(), &okE);
        if (!okS || !okE || e <= s) {
            QMessageBox::warning(this, QStringLiteral("错误"),
                                 QStringLiteral("第 %1 个片段时间无法解析").arg(segStart.size() + 1));
            return;
        }
        segStart.append(s);
        segEnd.append(e);
    }

    if (segStart.size() != m_table->rowCount()) {
        QMessageBox::warning(
            this, QStringLiteral("错误"),
            QStringLiteral("分割数据含 %1 段，但列表中有 %2 行。\n"
                           "请使片段文件与分割数据一一对应（可先用“按文件名排序”）。")
                .arg(segStart.size()).arg(m_table->rowCount()));
        return;
    }


    QList<RowData> rows;
    int clamped = 0;
    double k = 0;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        RowData d = rowData(r);

        d.start = 0;
        d.end = std::min<double>(d.duration,  segEnd[r]-segStart[r]);
        rows.append(d);
    }
    rebuildTable(rows);
    appendLog(QStringLiteral("已按 %1 的分割数据设置 %2 段裁剪")
                  .arg(QFileInfo(path).fileName())
                  .arg(rows.size()));
    if (clamped > 0)
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("有 %1 段的计算终点超出片段实际时长，已截断到文件尾。")
                                 .arg(clamped));
}

void MergeWidget::exportSplitData()
{
    if (m_table->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("列表为空"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出分割数据"), QStringLiteral("segments.json"),
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty())
        return;

    QJsonArray arr;
    double current = 0;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        const RowData d = rowData(r);
        QJsonObject o;

        o.insert(QStringLiteral("start"), current);
        current += effectiveDuration(d);
        o.insert(QStringLiteral("end"), current);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("segments"), arr);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法写入 %1").arg(path));
        return;
    }
    f.write(QJsonDocument(root).toJson());
    appendLog(QStringLiteral("已导出 %1 个片段到 %2").arg(arr.size()).arg(path));
}

void MergeWidget::importMergeData()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入合并数据"), QString(),
        QStringLiteral("JSON 文件 (*.json);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法打开 %1").arg(path));
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject() || !doc.object().value(QStringLiteral("files")).isArray()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("不是有效的合并数据文件（缺少 files 数组）"));
        return;
    }

    QList<RowData> rows;
    int missing = 0;
    const QJsonArray arr = doc.object().value(QStringLiteral("files")).toArray();

    QProgressDialog progress(QStringLiteral("正在读取媒体信息..."),
                             QStringLiteral("取消"), 0, arr.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setWindowTitle(QStringLiteral("导入合并数据"));
    progress.setMinimumDuration(300);
    progress.setValue(0);

    int done = 0;
    for (const QJsonValue &v : arr) {
        if (progress.wasCanceled())
            break;
        const QJsonObject o = v.toObject();
        const QString file = o.value(QStringLiteral("file")).toString();
        progress.setLabelText(QFileInfo(file).fileName());
        if (file.isEmpty() || !QFile::exists(file)) {
            ++missing;
            progress.setValue(++done);
            continue;
        }
        RowData d;
        d.path = file;
        d.start = o.value(QStringLiteral("start")).toDouble(0.0);
        d.end = o.value(QStringLiteral("end")).toDouble(-1.0);
        if (!m_ffprobePath.isEmpty()) {
            const MediaInfo info = probeMedia(m_ffprobePath, file);
            if (info.ok) {
                d.duration = info.duration;
                d.hasVideo = info.hasVideo;
                d.hasAudio = info.hasAudio;
            }
        }
        rows.append(d);
        progress.setValue(++done);
    }
    progress.close();
    rebuildTable(rows);
    if (missing > 0)
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("有 %1 个文件不存在，已跳过").arg(missing));
}

void MergeWidget::exportMergeData()
{
    if (m_table->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("列表为空"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出合并数据"), QStringLiteral("merge.json"),
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty())
        return;

    QJsonArray arr;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        const RowData d = rowData(r);
        QJsonObject o;
        o.insert(QStringLiteral("file"), d.path);
        o.insert(QStringLiteral("start"), d.start);
        o.insert(QStringLiteral("end"), effectiveEnd(d)); // 导出明确的有效终点
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("type"), QStringLiteral("vamergesplit-merge"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("files"), arr);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法写入 %1").arg(path));
        return;
    }
    f.write(QJsonDocument(root).toJson());
    appendLog(QStringLiteral("合并数据已导出：%1").arg(path));
}

// ---------- 合并 ----------

void MergeWidget::startMerge()
{
    if (m_ffmpegPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("未配置 ffmpeg，请先在“工具”菜单中设置"));
        return;
    }
    if (m_table->rowCount() < 2) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请至少添加两个文件"));
        return;
    }
    const QString outFile = m_outputEdit->text().trimmed();
    if (outFile.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("请指定输出文件"));
        return;
    }

    // 收集行数据（含裁剪配置）
    bool anyTrim = false;
    double totalDuration = 0;
    QList<RowData> entries;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        RowData d = rowData(r);
        d.end = effectiveEnd(d);
        if (d.end - d.start <= 1e-6) {
            QMessageBox::warning(this, QStringLiteral("错误"),
                                 QStringLiteral("%1 的有效时间段为空，请检查起止时间")
                                     .arg(QFileInfo(d.path).fileName()));
            return;
        }
        if (d.start > 1e-6 || d.end < d.duration - 1e-6)
            anyTrim = true;
        totalDuration += d.end - d.start;
        entries.append(d);
    }

    const bool reencode = m_modeCombo->currentData().toBool();
    const QString suffix = QFileInfo(outFile).suffix().toLower();
    QStringList args;

    if (anyTrim) {
        // ---------- 有裁剪：trim/atrim 滤镜 + 重编码，保证帧级精确 ----------
        if (!reencode) {
            QMessageBox::warning(
                this, QStringLiteral("错误"),
                QStringLiteral("已为一个或多个文件设置了起止时间（裁剪）。\n"
                               "无损直拷无法精确裁剪，请将输出模式改为“重编码”后再合并。"));
            return;
        }
        for (const RowData &d : entries) {
            if (d.hasVideo != entries.first().hasVideo || d.hasAudio != entries.first().hasAudio) {
                QMessageBox::warning(this, QStringLiteral("错误"),
                                     QStringLiteral("各文件的流组成不一致（有的含视频/有的不含），无法合并"));
                return;
            }
        }
        const bool audioOnlyOut = isAudioOnlyContainer(suffix);
        const bool withVideo = entries.first().hasVideo && !audioOnlyOut;
        const bool withAudio = entries.first().hasAudio;
        if (!withVideo && !withAudio) {
            QMessageBox::warning(this, QStringLiteral("错误"),
                                 QStringLiteral("输入文件没有可用的音视频流"));
            return;
        }

        args << QStringLiteral("-y");
        for (const RowData &d : entries)
            args << QStringLiteral("-i") << d.path;

        QStringList parts, outs;
        for (int i = 0; i < entries.size(); ++i) {
            const RowData &d = entries[i];
            const QString s = QString::number(d.start, 'f', 3);
            const QString t = QString::number(d.end, 'f', 3);
            if (withVideo) {
                parts << QStringLiteral("[%1:v]trim=start=%2:end=%3,setpts=PTS-STARTPTS[v%1]")
                             .arg(i).arg(s, t);
                outs << QStringLiteral("[v%1]").arg(i);
            }
            if (withAudio) {
                parts << QStringLiteral("[%1:a]atrim=start=%2:end=%3,asetpts=PTS-STARTPTS[a%1]")
                             .arg(i).arg(s, t);
                outs << QStringLiteral("[a%1]").arg(i);
            }
        }
        QString cat = outs.join(QString())
            + QStringLiteral("concat=n=%1:v=%2:a=%3")
                  .arg(entries.size()).arg(withVideo ? 1 : 0).arg(withAudio ? 1 : 0);
        const bool flacOut = (suffix == QLatin1String("flac"));
        if (withVideo)
            cat += QStringLiteral("[outv]");
        // 输出 flac 时先重新缓冲：atrim 会切出不足 16 样本的尾帧，
        // flac 编码器对此直接报 invalid block size 失败。
        if (withAudio)
            cat += flacOut ? QStringLiteral("[cat_a]") : QStringLiteral("[outa]");
        parts << cat;
        if (withAudio && flacOut)
            parts << QStringLiteral("[cat_a]asetnsamples=4096:p=0[outa]");

        args << QStringLiteral("-filter_complex") << parts.join(QLatin1Char(';'));
        if (withVideo)
            args << QStringLiteral("-map") << QStringLiteral("[outv]")
                 << QStringLiteral("-c:v") << QStringLiteral("libx264")
                 << QStringLiteral("-preset") << QStringLiteral("veryfast")
                 << QStringLiteral("-crf") << QStringLiteral("18")
                 << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
        if (withAudio) {
            args << QStringLiteral("-map") << QStringLiteral("[outa]");
            args << audioEncoderArgs(suffix);
        }
        args << outFile;
    } else {
        // ---------- 无裁剪：concat demuxer（无损直拷或整体重编码） ----------
        QStringList lines;
        for (const RowData &d : entries) {
            QString path = d.path;
            path.replace(QLatin1Char('\''), QStringLiteral("'\\''")); // ffmpeg concat 转义
            lines << QStringLiteral("file '%1'").arg(path);
        }
        const QString listFile = QDir(QFileInfo(outFile).absolutePath())
                                     .filePath(QStringLiteral(".vamergesplit_concat.txt"));
        QFile f(listFile);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, QStringLiteral("错误"),
                                 QStringLiteral("无法创建临时列表文件 %1").arg(listFile));
            return;
        }
        f.write(lines.join(QLatin1Char('\n')).toUtf8());
        f.close();

        args << QStringLiteral("-y")
             << QStringLiteral("-f") << QStringLiteral("concat")
             << QStringLiteral("-safe") << QStringLiteral("0")
             << QStringLiteral("-i") << listFile;
        // flac 容器在 stream copy 时会把源的 STREAMINFO.total_samples 带进输出头，
        // 合并后时长元数据会显示为错误值（音频数据本身是对的），
        // 因此输出到 flac 时即使选了「无损直拷」也改为 flac 重编码（无损，无音质损失）。
        const bool flacOutCopyFix = (suffix == QLatin1String("flac"));
        if (!reencode && flacOutCopyFix)
            appendLog(QStringLiteral("注意：输出为 flac，本次改用 flac 无损重编码以写入正确的时长元数据。"));
        if (!reencode && !flacOutCopyFix) {
            args << QStringLiteral("-map") << QStringLiteral("0")
                 << QStringLiteral("-c") << QStringLiteral("copy");
        } else {
            if (isAudioOnlyContainer(suffix)) {
                args << QStringLiteral("-map") << QStringLiteral("0:a?")
                     << QStringLiteral("-vn") << audioEncoderArgs(suffix);
            } else {
                args << QStringLiteral("-map") << QStringLiteral("0:v?")
                     << QStringLiteral("-map") << QStringLiteral("0:a?")
                     << QStringLiteral("-c:v") << QStringLiteral("libx264")
                     << QStringLiteral("-preset") << QStringLiteral("veryfast")
                     << QStringLiteral("-crf") << QStringLiteral("18")
                     << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
                     << audioEncoderArgs(suffix);
            }
        }
        args << outFile;
    }

    FfmpegTask task;
    task.label = QStringLiteral("合并 %1 个文件 -> %2")
                     .arg(entries.size()).arg(QFileInfo(outFile).fileName());
    task.args = args;
    task.duration = totalDuration;

    m_startButton->setEnabled(false);
    m_cancelButton->setEnabled(true);
    m_progressBar->setValue(0);
    m_logView->clear();
    m_runner->start(m_ffmpegPath, {task});
}

void MergeWidget::cancelMerge()
{
    m_runner->cancel();
}

void MergeWidget::appendLog(const QString &line)
{
    m_logView->appendPlainText(line);
}
