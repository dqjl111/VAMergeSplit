#include "WaveformWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QWheelEvent>
#include <QtEndian>

// ---------------- WaveformWidget ----------------

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setAutoFillBackground(false);
}

void WaveformWidget::setWaveform(QVector<qint16> mins, QVector<qint16> maxs, double duration)
{
    m_mins = std::move(mins);
    m_maxs = std::move(maxs);
    m_duration = duration;
    m_position = 0;
    m_zoom = 1.0;
    m_viewStart = 0;
    m_message.clear();
    emit viewChanged();
    update();
}

void WaveformWidget::clear()
{
    m_mins.clear();
    m_maxs.clear();
    m_duration = 0;
    m_position = 0;
    m_zoom = 1.0;
    m_viewStart = 0;
    emit viewChanged();
    update();
}

void WaveformWidget::setMessage(const QString &message)
{
    m_message = message;
    update();
}

void WaveformWidget::setPosition(double seconds)
{
    if (qFuzzyCompare(m_position, seconds))
        return;
    m_position = seconds;
    update();
}

double WaveformWidget::visibleDuration() const
{
    return m_duration > 0 ? m_duration / m_zoom : 0;
}

double WaveformWidget::maxViewStart() const
{
    return qMax(0.0, m_duration - visibleDuration());
}

void WaveformWidget::setZoom(double factor)
{
    factor = qBound(1.0, factor, 200.0);
    if (qFuzzyCompare(factor, m_zoom))
        return;
    const double center = m_viewStart + visibleDuration() / 2;
    m_zoom = factor;
    setViewStart(center - visibleDuration() / 2); // 保持视图中心不动
    emit viewChanged();
    update();
}

void WaveformWidget::setViewStart(double seconds)
{
    seconds = qBound(0.0, seconds, maxViewStart());
    if (qFuzzyCompare(m_viewStart, seconds))
        return;
    m_viewStart = seconds;
    emit viewChanged();
    update();
}

void WaveformWidget::setViewStartFraction(double fraction)
{
    setViewStart(qBound(0.0, fraction, 1.0) * maxViewStart());
}

double WaveformWidget::secondsFromX(int x) const
{
    const double vis = visibleDuration();
    if (vis <= 0 || width() <= 0)
        return 0;
    return qBound(0.0, m_viewStart + qBound(0, x, width()) * vis / width(), m_duration);
}

void WaveformWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_duration > 0 && event->button() == Qt::LeftButton)
        emit seekRequested(secondsFromX(event->pos().x()));
    QWidget::mousePressEvent(event);
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_duration > 0 && (event->buttons() & Qt::LeftButton))
        emit seekRequested(secondsFromX(event->pos().x()));
    QWidget::mouseMoveEvent(event);
}

void WaveformWidget::wheelEvent(QWheelEvent *event)
{
    if (m_duration <= 0) {
        QWidget::wheelEvent(event);
        return;
    }
    // 滚轮缩放，保持光标下的时间点不动
    const double anchor = secondsFromX(int(event->position().x()));
    const double frac = width() > 0 ? event->position().x() / width() : 0.5;
    m_zoom = qBound(1.0, m_zoom * (event->angleDelta().y() > 0 ? 1.25 : 0.8), 200.0);
    setViewStart(anchor - visibleDuration() * frac);
    emit viewChanged();
    update();
    event->accept();
}

void WaveformWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 30));

    if (!m_message.isEmpty()) {
        p.setPen(QColor(150, 150, 150));
        p.drawText(rect(), Qt::AlignCenter, m_message);
    }

    if (m_mins.isEmpty() || m_duration <= 0)
        return;

    const int w = width();
    const int h = height();
    const int cy = h / 2;
    const int half = h / 2 - 6;
    const double vis = visibleDuration();
    const int cols = m_mins.size();

    // 中线
    p.setPen(QColor(90, 90, 90));
    p.drawLine(0, cy, w, cy);

    // 波形（只画可见窗口内的部分）
    const double pixelsPerCol = double(w) * m_duration / (vis * cols);
    if (pixelsPerCol >= 1.0) {
        // 放大：每列跨度超过 1 像素，画细折线包络避免“变粗”
        const int c0 = qMax(0, int(m_viewStart / m_duration * cols));
        const int c1 = qMin(cols - 1, int((m_viewStart + vis) / m_duration * cols) + 1);
        QPolygonF top, bottom;
        top.reserve(c1 - c0 + 1);
        bottom.reserve(c1 - c0 + 1);
        for (int c = c0; c <= c1; ++c) {
            const double t = (c + 0.5) / cols * m_duration;
            const double x = (t - m_viewStart) / vis * w;
            top << QPointF(x, cy - m_maxs[c] / 32768.0 * half);
            bottom << QPointF(x, cy - m_mins[c] / 32768.0 * half);
        }
        p.setPen(QPen(QColor(79, 195, 247), 1));
        p.drawPolyline(top);
        p.drawPolyline(bottom);
    } else {
        // 缩小：每像素聚合并其覆盖的列范围，取 min/max
        p.setPen(QColor(79, 195, 247));
        for (int x = 0; x < w; ++x) {
            const double t0 = m_viewStart + double(x) / w * vis;
            const double t1 = m_viewStart + double(x + 1) / w * vis;
            const int c0 = qBound(0, int(t0 / m_duration * cols), cols - 1);
            const int c1 = qBound(c0, int(t1 / m_duration * cols), cols - 1);
            qint16 mn = INT16_MAX, mx = INT16_MIN;
            for (int c = c0; c <= c1; ++c) {
                mn = qMin(mn, m_mins[c]);
                mx = qMax(mx, m_maxs[c]);
            }
            const int y1 = cy - int(mx / 32768.0 * half);
            const int y2 = cy - int(mn / 32768.0 * half);
            p.drawLine(x, y1, x, qMax(y1 + 1, y2));
        }
    }

    // 播放游标（在可见窗口内才绘制）
    if (m_position >= m_viewStart && m_position <= m_viewStart + vis) {
        const int px = int((m_position - m_viewStart) / vis * w);
        p.setPen(QPen(QColor(255, 82, 82), 2));
        p.drawLine(px, 0, px, h);
    }
}

// ---------------- WaveformLoader ----------------

WaveformLoader::WaveformLoader(QObject *parent)
    : QObject(parent)
{
}

WaveformLoader::~WaveformLoader()
{
    cancel();
}

void WaveformLoader::load(const QString &ffmpegPath, const QString &filePath, double duration)
{
    cancel();
    m_duration = duration;

    // 限制总采样数（约 4M 样本 = 8MB），长音频自动降低采样率
    int rate = 8000;
    if (duration > 0)
        rate = qBound(500, int(4000000.0 / duration), 8000);

    m_process = new QProcess(this);
    m_process->setProgram(ffmpegPath);
    m_process->setArguments({
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-i"), filePath,
        QStringLiteral("-vn"),
        QStringLiteral("-ac"), QStringLiteral("1"),
        QStringLiteral("-ar"), QString::number(rate),
        QStringLiteral("-f"), QStringLiteral("s16le"),
        QStringLiteral("-")
    });

    connect(m_process, &QProcess::finished, this,
            [this, duration](int exitCode, QProcess::ExitStatus status) {
                QProcess *proc = qobject_cast<QProcess *>(sender());
                if (!proc)
                    return;
                const QByteArray pcm = proc->readAllStandardOutput();
                const QString err = QString::fromLocal8Bit(proc->readAllStandardError()).trimmed();
                proc->deleteLater();
                if (m_process == proc)
                    m_process = nullptr;

                if (status != QProcess::NormalExit)
                    return; // 被取消，忽略
                if (exitCode != 0 || pcm.size() < 4) {
                    emit failed(err.isEmpty() ? QStringLiteral("ffmpeg 解码失败") : err);
                    return;
                }

                const int sampleCount = pcm.size() / 2;
                const qint16 *samples = reinterpret_cast<const qint16 *>(pcm.constData());

                constexpr int kColumns = 5000;
                const int columns = qMin(kColumns, sampleCount);
                QVector<qint16> mins(columns, 0), maxs(columns, 0);
                for (int c = 0; c < columns; ++c) {
                    const int from = int(qint64(c) * sampleCount / columns);
                    const int to = int(qint64(c + 1) * sampleCount / columns);
                    qint16 mn = INT16_MAX, mx = INT16_MIN;
                    for (int i = from; i < to; ++i) {
                        const qint16 s = qFromLittleEndian(samples[i]);
                        mn = qMin(mn, s);
                        mx = qMax(mx, s);
                    }
                    mins[c] = (mn == INT16_MAX) ? qint16(0) : mn;
                    maxs[c] = (mx == INT16_MIN) ? qint16(0) : mx;
                }
                emit finished(mins, maxs, duration);
            });

    m_process->start();
}

void WaveformLoader::cancel()
{
    if (!m_process)
        return;
    m_process->disconnect(this);
    m_process->kill();
    m_process->deleteLater();
    m_process = nullptr;
}
