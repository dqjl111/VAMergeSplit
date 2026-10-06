#include "SegmentModel.h"

#include "TimeUtils.h"

SegmentModel::SegmentModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int SegmentModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_segments.size();
}

int SegmentModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

QVariant SegmentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_segments.size())
        return {};

    const Segment &seg = m_segments[index.row()];

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case ColIndex:
            return index.row() + 1;
        case ColStart:
            return TimeUtils::formatTime(seg.start);
        case ColEnd:
            return TimeUtils::formatTime(seg.end);
        case ColDuration:
            return TimeUtils::formatTime(seg.end - seg.start);
        case ColStartFrame:
            return m_fps > 0 ? QVariant(TimeUtils::frameFromTime(seg.start, m_fps))
                             : QVariant(QStringLiteral("-"));
        case ColEndFrame:
            return m_fps > 0 ? QVariant(TimeUtils::frameFromTime(seg.end, m_fps))
                             : QVariant(QStringLiteral("-"));
        }
    }
    if (role == Qt::TextAlignmentRole && index.column() == ColIndex)
        return int(Qt::AlignCenter);
    if (role == Qt::ToolTipRole) {
        switch (index.column()) {
        case ColStart:
        case ColEnd:
            return QStringLiteral("支持格式：秒、MM:SS、HH:MM:SS.mmm");
        case ColStartFrame:
        case ColEndFrame:
            return m_fps > 0 ? QStringLiteral("帧号（整数），与对应时间列自动同步")
                             : QStringLiteral("音频文件无帧率，不可编辑");
        }
    }
    return {};
}

bool SegmentModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || index.row() >= m_segments.size())
        return false;

    Segment &seg = m_segments[index.row()];
    bool ok = false;
    double newTime = 0;

    switch (index.column()) {
    case ColStart:
    case ColEnd: {
        newTime = TimeUtils::parseTime(value.toString(), &ok);
        break;
    }
    case ColStartFrame:
    case ColEndFrame: {
        if (m_fps <= 0)
            return false;
        const qint64 frame = value.toString().toLongLong(&ok);
        if (ok && frame >= 0)
            newTime = TimeUtils::timeFromFrame(frame, m_fps);
        else
            ok = false;
        break;
    }
    default:
        return false;
    }
    if (!ok)
        return false;

    // 约束：0 <= start < end <= 媒体时长
    const double maxEnd = m_mediaDuration > 0 ? m_mediaDuration : 1e12;
    switch (index.column()) {
    case ColStart:
    case ColStartFrame:
        if (newTime >= seg.end)
            return false;
        seg.start = qBound(0.0, newTime, seg.end - 0.001);
        break;
    case ColEnd:
    case ColEndFrame:
        if (newTime <= seg.start || newTime > maxEnd + 0.001)
            return false;
        seg.end = newTime;
        break;
    }

    emit dataChanged(this->index(index.row(), 0),
                     this->index(index.row(), ColCount - 1));
    return true;
}

QVariant SegmentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};
    switch (section) {
    case ColIndex:     return QStringLiteral("序号");
    case ColStart:     return QStringLiteral("开始时间");
    case ColEnd:       return QStringLiteral("结束时间");
    case ColDuration:  return QStringLiteral("时长");
    case ColStartFrame:return QStringLiteral("起始帧");
    case ColEndFrame:  return QStringLiteral("结束帧");
    }
    return {};
}

Qt::ItemFlags SegmentModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (!index.isValid())
        return f;
    switch (index.column()) {
    case ColStart:
    case ColEnd:
        f |= Qt::ItemIsEditable;
        break;
    case ColStartFrame:
    case ColEndFrame:
        if (m_fps > 0)
            f |= Qt::ItemIsEditable;
        break;
    }
    return f;
}

void SegmentModel::setFps(double fps)
{
    if (qFuzzyCompare(m_fps, fps))
        return;
    beginResetModel();
    m_fps = fps;
    endResetModel();
}

void SegmentModel::setSegments(const QVector<Segment> &segments)
{
    beginResetModel();
    m_segments = segments;
    endResetModel();
}

void SegmentModel::splitEvenly(double totalDuration, int n)
{
    QVector<Segment> result;
    if (n > 0 && totalDuration > 0) {
        result.reserve(n);
        const double step = totalDuration / n;
        for (int i = 0; i < n; ++i) {
            Segment seg;
            seg.start = i * step;
            seg.end = (i == n - 1) ? totalDuration : (i + 1) * step;
            result.append(seg);
        }
    }
    setSegments(result);
}

void SegmentModel::alignAsMinimax(double totalDuration, bool keepContinue)
{
    QVector<Segment> result = m_segments;
    double duration = 0;
    for(int i=0; i<result.size(); ++i){
        duration = result[i].end - result[i].start;
        qWarning() << "org duration:" << duration;
        if(duration >0 && i < (result.size()-1)){
            result[i].end = result[i].start +  std::min<double>(17*ceil((24*duration-5)/17)+5,362)/24;
        }

        if(result[i].start > totalDuration){
            result[i].start = totalDuration;
        }

        if(result[i].end > totalDuration){
            result[i].end = totalDuration;
        }

        if(keepContinue){
            if((i+1) < result.size()){
                result[i+1].start = result[i].end;
                if(result[i+1].end <= result[i+1].start){
                    result[i+1].end = result[i+1].start;
                }
            }
        }
    }
    setSegments(result);
}

bool SegmentModel::removeRowAt(int row)
{
    if (row < 0 || row >= m_segments.size())
        return false;
    beginRemoveRows({}, row, row);
    m_segments.removeAt(row);
    endRemoveRows();
    return true;
}

bool SegmentModel::moveRowAt(int row, int delta)
{
    const int target = row + delta;
    if (row < 0 || row >= m_segments.size() || target < 0 || target >= m_segments.size())
        return false;
    beginMoveRows({}, row, row, {}, target > row ? target + 1 : target);
    m_segments.swapItemsAt(row, target);
    endMoveRows();
    return true;
}

bool SegmentModel::insertSegment(int row, const Segment &seg)
{
    row = qBound(0, row, m_segments.size());
    beginInsertRows({}, row, row);
    m_segments.insert(row, seg);
    endInsertRows();
    return true;
}

bool SegmentModel::splitRowAt(int row, double time)
{
    if (row < 0 || row >= m_segments.size())
        return false;
    const Segment seg = m_segments[row];
    if (time <= seg.start + 0.001 || time >= seg.end - 0.001)
        return false;
    beginInsertRows({}, row + 1, row + 1);
    m_segments[row].end = time;
    m_segments.insert(row + 1, Segment{time, seg.end});
    endInsertRows();
    emit dataChanged(index(row, 0), index(row + 1, ColCount - 1));
    return true;
}

QString SegmentModel::validate() const
{
    if (m_segments.isEmpty())
        return QStringLiteral("片段列表为空");
    for (int i = 0; i < m_segments.size(); ++i) {
        const Segment &seg = m_segments[i];
        if (seg.start < 0 || seg.end <= seg.start)
            return QStringLiteral("第 %1 段时间不合法（开始必须小于结束）").arg(i + 1);
        if (m_mediaDuration > 0 && seg.end > m_mediaDuration + 0.001)
            return QStringLiteral("第 %1 段超出媒体总时长").arg(i + 1);
    }
    return {};
}
