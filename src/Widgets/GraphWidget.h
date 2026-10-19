#pragma once

#include "Models/VehicleSample.h"

#include <QImage>
#include <QList>
#include <QWidget>

class QChart;
class QComboBox;
class QGraphicsLineItem;
class QGraphicsSimpleTextItem;
class QLabel;
class QLineSeries;
class QValueAxis;
class GraphChartView;

// Graphique temps réel multi-capteurs (QtCharts) :
// molette = zoom, clic-glisser = zoom sur zone, double-clic = réinitialiser,
// survol = curseur d'analyse avec les valeurs au point survolé.
class GraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GraphWidget(QWidget* parent = nullptr);

    void addSample(const VehicleSample& sample);
    void loadSamples(const QList<VehicleSample>& samples);   // relecture d'historique
    void clear();

    QImage capture(const QSize& size = QSize(1400, 700)) const;
    void applyTheme(bool dark);

private:
    struct Point
    {
        double t;
        VehicleSample s;
    };

    void appendPoint(const Point& p);
    void updateRanges();
    void resetZoom();
    void onHover(const QPointF& chartPos, bool inside);

    QChart* m_chart;
    GraphChartView* m_view;
    QValueAxis* m_axisX;
    QValueAxis* m_axisRpm;
    QValueAxis* m_axisOther;

    QLineSeries* m_rpm;
    QLineSeries* m_speed;
    QLineSeries* m_coolant;
    QLineSeries* m_throttle;
    QLineSeries* m_load;

    QComboBox* m_windowCombo;
    QLabel* m_cursorLabel;
    QGraphicsLineItem* m_cursorLine = nullptr;

    QList<Point> m_points;
    QDateTime m_origin;
    bool m_zoomed = false;
    int m_maxPoints = 20000;
};
