#include "ui/speed_graph.h"

#include <QColor>
#include <QFile>
#include <QIODevice>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QSizePolicy>
#include <QTextStream>
#include <algorithm>
#include <cmath>

namespace remo {
namespace ui {

class SpeedGraph::Impl {};

SpeedGraph::SpeedGraph(QWidget* parent)
    : QWidget(parent)
    , d(std::make_unique<Impl>())
{
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

SpeedGraph::~SpeedGraph() = default;

void SpeedGraph::addPoint(double timestamp, double bytesPerSec) {
    SpeedPoint point;
    point.timestamp = timestamp;
    point.bytesPerSec = bytesPerSec;
    history.push_back(point);
    const double cutoff = timestamp - maxDuration;
    history.erase(std::remove_if(history.begin(), history.end(), [cutoff](const SpeedPoint& item) {
        return item.timestamp < cutoff;
    }), history.end());
    update();
}

void SpeedGraph::clear() {
    history.clear();
    update();
}

void SpeedGraph::setHistoryDuration(double seconds) {
    maxDuration = std::max(10.0, seconds);
    update();
}

void SpeedGraph::exportToCsv(const std::string& filePath) {
    QFile file(QString::fromStdString(filePath));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
    out << "timestamp,bytes_per_sec\n";
    for (const auto& point : history) {
        out << point.timestamp << "," << point.bytesPerSec << "\n";
    }
}

void SpeedGraph::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF plot = rect().adjusted(12, 12, -12, -20);
    painter.fillRect(rect(), palette().base());
    painter.setPen(QPen(QColor("#3A3D45"), 1));
    painter.drawRoundedRect(plot, 6, 6);

    if (history.size() < 2) {
        painter.setPen(QColor("#9B9EA5"));
        painter.drawText(plot, Qt::AlignCenter, tr("لا توجد بيانات سرعة بعد"));
        return;
    }

    const double start = history.front().timestamp;
    const double end = std::max(history.back().timestamp, start + 1.0);
    const double maxSpeed = std::max(1.0, std::max_element(history.begin(), history.end(),
        [](const SpeedPoint& a, const SpeedPoint& b) { return a.bytesPerSec < b.bytesPerSec; })->bytesPerSec);

    QPainterPath path;
    for (size_t i = 0; i < history.size(); ++i) {
        const double xRatio = (history[i].timestamp - start) / (end - start);
        const double yRatio = history[i].bytesPerSec / maxSpeed;
        const QPointF point(plot.left() + xRatio * plot.width(),
                            plot.bottom() - yRatio * plot.height());
        if (i == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }

    painter.setPen(QPen(QColor("#5B8DEF"), 2.5));
    painter.drawPath(path);
    painter.setPen(QColor("#9B9EA5"));
    painter.drawText(rect().adjusted(12, 0, -12, -4), Qt::AlignBottom | Qt::AlignLeft,
                     tr("الحد الأعلى: %1 MB/s").arg(maxSpeed / 1024.0 / 1024.0, 0, 'f', 2));
}

void SpeedGraph::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
}

} // namespace ui
} // namespace remo
