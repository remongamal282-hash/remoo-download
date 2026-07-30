#ifndef REMO_DOWNLOAD_UI_SPEED_GRAPH_H
#define REMO_DOWNLOAD_UI_SPEED_GRAPH_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <QWidget>

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QResizeEvent;
class QTimer;
QT_END_NAMESPACE

namespace remo {
namespace ui {

struct SpeedPoint {
    double timestamp = 0.0;
    double bytesPerSec = 0.0;
};

class SpeedGraph : public QWidget {
public:
    explicit SpeedGraph(QWidget* parent = nullptr);
    ~SpeedGraph();

    void addPoint(double timestamp, double bytesPerSec);
    void clear();
    void setHistoryDuration(double seconds);
    void exportToCsv(const std::string& filePath);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    std::vector<SpeedPoint> history;
    double maxDuration = 300.0;
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace ui
} // namespace remo

#endif // REMO_DOWNLOAD_UI_SPEED_GRAPH_H
