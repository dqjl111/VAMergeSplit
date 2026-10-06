#pragma once

#include <QAbstractTableModel>
#include <QVector>

struct Segment {
    double start = 0; // 秒
    double end = 0;   // 秒
};

// 片段表格模型：序号 | 开始时间 | 结束时间 | 时长 | 起始帧 | 结束帧
// 时间与帧两列均可编辑，互相同步（帧列需要有效帧率）
class SegmentModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColIndex = 0,
        ColStart,
        ColEnd,
        ColDuration,
        ColStartFrame,
        ColEndFrame,
        ColCount
    };

    explicit SegmentModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void setFps(double fps);
    double fps() const { return m_fps; }

    void setMediaDuration(double duration) { m_mediaDuration = duration; }
    double mediaDuration() const { return m_mediaDuration; }

    const QVector<Segment> &segments() const { return m_segments; }
    void setSegments(const QVector<Segment> &segments);

    // 将媒体总长平均分成 n 段
    void splitEvenly(double totalDuration, int n);
    void alignAsMinimax(double totalDuration, bool keepContinue);

    bool removeRowAt(int row);
    bool moveRowAt(int row, int delta); // delta = -1 上移 / +1 下移
    bool insertSegment(int row, const Segment &seg);
    // 在 time 处把 row 段一分为二；time 不在该段内部则失败
    bool splitRowAt(int row, double time);

    // 校验所有片段合法（0 <= start < end <= 媒体时长）
    QString validate() const;

private:
    QVector<Segment> m_segments;
    double m_fps = 0;
    double m_mediaDuration = 0;
};
