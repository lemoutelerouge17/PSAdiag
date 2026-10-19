#include "GraphWidget.h"

#include <QAbstractSeries>
#include <QChart>
#include <QChartView>
#include <QComboBox>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QLegendMarker>
#include <QLineSeries>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QValueAxis>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <functional>

// ---------------------------------------------------------------------------
// Vue graphique avec zoom horizontal à la molette et curseur d'analyse
// ---------------------------------------------------------------------------
class GraphChartView : public QChartView
{
public:
    using QChartView::QChartView;

    std::function<void(double factor, double centerX)> onWheelZoom;
    std::function<void()> onRubberBandZoom;
    std::function<void()> onResetZoom;
    std::function<void(const QPointF& value, bool inside)> onHover;

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        if (!chart() || chart()->series().isEmpty())
            return;
        const QPointF chartPos = chart()->mapFromScene(mapToScene(event->position().toPoint()));
        const QPointF value = chart()->mapToValue(chartPos, chart()->series().first());
        const double factor = event->angleDelta().y() > 0 ? 0.8 : 1.25;
        if (onWheelZoom)
            onWheelZoom(factor, value.x());
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        QChartView::mouseReleaseEvent(event);
        if (event->button() == Qt::LeftButton && onRubberBandZoom)
            onRubberBandZoom();
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (onResetZoom)
            onResetZoom();
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        QChartView::mouseMoveEvent(event);
        if (!onHover || !chart() || chart()->series().isEmpty())
            return;
        const QPointF scenePos = mapToScene(event->position().toPoint());
        const QPointF chartPos = chart()->mapFromScene(scenePos);
        const bool inside = chart()->plotArea().contains(chartPos);
        onHover(chart()->mapToValue(chartPos, chart()->series().first()), inside);
    }

    void leaveEvent(QEvent* event) override
    {
        QChartView::leaveEvent(event);
        if (onHover)
            onHover(QPointF(), false);
    }
};

// ---------------------------------------------------------------------------

namespace
{
QLineSeries* makeSeries(const QString& name, const QColor& color)
{
    auto* s = new QLineSeries;
    s->setName(name);
    QPen pen(color);
    pen.setWidthF(1.8);
    s->setPen(pen);
    s->setUseOpenGL(false);
    return s;
}

QString fmt(double v, int d)
{
    return VehicleSample::valid(v) ? QString::number(v, 'f', d) : QStringLiteral("--");
}
}

GraphWidget::GraphWidget(QWidget* parent)
    : QWidget(parent)
{
    m_chart = new QChart;
    m_chart->setAnimationOptions(QChart::NoAnimation);
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);
    m_chart->setMargins(QMargins(4, 4, 4, 4));

    m_rpm      = makeSeries(tr("Régime (tr/min)"), QColor(230, 70, 60));
    m_speed    = makeSeries(tr("Vitesse (km/h)"), QColor(66, 133, 244));
    m_coolant  = makeSeries(tr("Temp. moteur (°C)"), QColor(250, 160, 30));
    m_throttle = makeSeries(tr("Papillon (%)"), QColor(60, 180, 90));
    m_load     = makeSeries(tr("Charge (%)"), QColor(170, 90, 210));

    for (QLineSeries* s : { m_rpm, m_speed, m_coolant, m_throttle, m_load })
        m_chart->addSeries(s);

    m_axisX = new QValueAxis;
    m_axisX->setTitleText(tr("Temps (s)"));
    m_axisX->setLabelFormat(QStringLiteral("%.0f"));
    m_axisX->setRange(0, 60);

    m_axisRpm = new QValueAxis;
    m_axisRpm->setTitleText(tr("tr/min"));
    m_axisRpm->setLabelFormat(QStringLiteral("%.0f"));
    m_axisRpm->setRange(0, 7000);

    m_axisOther = new QValueAxis;
    m_axisOther->setTitleText(tr("km/h · °C · %"));
    m_axisOther->setLabelFormat(QStringLiteral("%.0f"));
    m_axisOther->setRange(-20, 200);

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisRpm, Qt::AlignLeft);
    m_chart->addAxis(m_axisOther, Qt::AlignRight);

    m_rpm->attachAxis(m_axisX);
    m_rpm->attachAxis(m_axisRpm);
    for (QLineSeries* s : { m_speed, m_coolant, m_throttle, m_load }) {
        s->attachAxis(m_axisX);
        s->attachAxis(m_axisOther);
    }

    // Clic sur la légende : afficher / masquer la courbe.
    for (QLegendMarker* marker : m_chart->legend()->markers()) {
        connect(marker, &QLegendMarker::clicked, this, [this, marker]() {
            QAbstractSeries* series = marker->series();
            series->setVisible(!series->isVisible());
            marker->setVisible(true);
            marker->setLabelBrush(series->isVisible() ? QBrush(m_chart->legend()->labelColor())
                                                      : QBrush(Qt::gray));
            QColor c = marker->brush().color();
            c.setAlphaF(series->isVisible() ? 1.0 : 0.3);
            marker->setBrush(c);
        });
    }

    m_view = new GraphChartView(m_chart);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setRubberBand(QChartView::HorizontalRubberBand);
    m_view->setMouseTracking(true);

    m_view->onWheelZoom = [this](double factor, double center) {
        const double min = m_axisX->min();
        const double max = m_axisX->max();
        const double newMin = center - (center - min) * factor;
        const double newMax = center + (max - center) * factor;
        if (newMax - newMin < 2)
            return;
        m_axisX->setRange(qMax(0.0, newMin), newMax);
        m_zoomed = true;
    };
    m_view->onRubberBandZoom = [this]() { m_zoomed = true; };
    m_view->onResetZoom = [this]() { resetZoom(); };
    m_view->onHover = [this](const QPointF& v, bool inside) { onHover(v, inside); };

    m_cursorLine = new QGraphicsLineItem(m_chart);
    m_cursorLine->setPen(QPen(QColor(128, 128, 128), 1, Qt::DashLine));
    m_cursorLine->setZValue(100);
    m_cursorLine->hide();

    // --- Barre d'outils ------------------------------------------------
    m_windowCombo = new QComboBox;
    m_windowCombo->addItem(tr("30 s"), 30);
    m_windowCombo->addItem(tr("1 min"), 60);
    m_windowCombo->addItem(tr("2 min"), 120);
    m_windowCombo->addItem(tr("5 min"), 300);
    m_windowCombo->addItem(tr("10 min"), 600);
    m_windowCombo->addItem(tr("Tout"), 0);
    m_windowCombo->setCurrentIndex(1);
    connect(m_windowCombo, &QComboBox::currentIndexChanged, this, [this]() { resetZoom(); });

    auto* resetButton = new QPushButton(tr("Réinitialiser le zoom"));
    connect(resetButton, &QPushButton::clicked, this, &GraphWidget::resetZoom);

    auto* clearButton = new QPushButton(tr("Effacer"));
    connect(clearButton, &QPushButton::clicked, this, &GraphWidget::clear);

    m_cursorLabel = new QLabel(tr("Molette : zoom · Glisser : zoom sur zone · Double-clic : réinitialiser"));
    m_cursorLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* bar = new QHBoxLayout;
    bar->addWidget(new QLabel(tr("Fenêtre :")));
    bar->addWidget(m_windowCombo);
    bar->addWidget(resetButton);
    bar->addWidget(clearButton);
    bar->addSpacing(12);
    bar->addWidget(m_cursorLabel, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(bar);
    layout->addWidget(m_view, 1);
}

void GraphWidget::appendPoint(const Point& p)
{
    auto add = [&](QLineSeries* series, double v) {
        if (VehicleSample::valid(v))
            series->append(p.t, v);
    };
    add(m_rpm, p.s.rpm);
    add(m_speed, p.s.speed);
    add(m_coolant, p.s.coolant);
    add(m_throttle, p.s.throttle);
    add(m_load, p.s.engineLoad);
}

void GraphWidget::addSample(const VehicleSample& sample)
{
    const QDateTime ts = sample.timestamp.isValid() ? sample.timestamp : QDateTime::currentDateTime();
    if (!m_origin.isValid())
        m_origin = ts;

    const Point p{ m_origin.msecsTo(ts) / 1000.0, sample };
    m_points.append(p);
    appendPoint(p);

    // Limite mémoire : on retire les points les plus anciens.
    if (m_points.size() > m_maxPoints) {
        const int excess = int(m_points.size()) - m_maxPoints;
        m_points.remove(0, excess);
        const double tMin = m_points.first().t;
        for (QLineSeries* s : { m_rpm, m_speed, m_coolant, m_throttle, m_load }) {
            int n = 0;
            while (n < s->count() && s->at(n).x() < tMin)
                ++n;
            if (n > 0)
                s->removePoints(0, n);
        }
    }

    if (!m_zoomed)
        updateRanges();
}

void GraphWidget::loadSamples(const QList<VehicleSample>& samples)
{
    clear();
    if (samples.isEmpty())
        return;

    m_origin = samples.first().timestamp;

    QList<QPointF> rpm, speed, coolant, throttle, load;
    for (const VehicleSample& s : samples) {
        const Point p{ m_origin.msecsTo(s.timestamp) / 1000.0, s };
        m_points.append(p);
        if (VehicleSample::valid(s.rpm))        rpm << QPointF(p.t, s.rpm);
        if (VehicleSample::valid(s.speed))      speed << QPointF(p.t, s.speed);
        if (VehicleSample::valid(s.coolant))    coolant << QPointF(p.t, s.coolant);
        if (VehicleSample::valid(s.throttle))   throttle << QPointF(p.t, s.throttle);
        if (VehicleSample::valid(s.engineLoad)) load << QPointF(p.t, s.engineLoad);
    }
    // replace() est bien plus rapide que des append() successifs.
    m_rpm->replace(rpm);
    m_speed->replace(speed);
    m_coolant->replace(coolant);
    m_throttle->replace(throttle);
    m_load->replace(load);

    m_windowCombo->setCurrentIndex(m_windowCombo->findData(0));
    resetZoom();
}

void GraphWidget::clear()
{
    m_points.clear();
    m_origin = QDateTime();
    for (QLineSeries* s : { m_rpm, m_speed, m_coolant, m_throttle, m_load })
        s->clear();
    m_cursorLine->hide();
    resetZoom();
}

void GraphWidget::resetZoom()
{
    m_zoomed = false;
    m_chart->zoomReset();
    updateRanges();
}

void GraphWidget::updateRanges()
{
    const double last = m_points.isEmpty() ? 0 : m_points.last().t;
    const double first = m_points.isEmpty() ? 0 : m_points.first().t;
    const int window = m_windowCombo->currentData().toInt();

    if (window <= 0) {
        m_axisX->setRange(first, qMax(first + 10, last));
    } else {
        const double max = qMax(double(window), last);
        m_axisX->setRange(max - window, max);
    }

    // Échelle du régime adaptée au maximum observé.
    double maxRpm = 0;
    for (const Point& p : std::as_const(m_points))
        if (VehicleSample::valid(p.s.rpm))
            maxRpm = qMax(maxRpm, p.s.rpm);
    const double rpmTop = qMax(4000.0, std::ceil((maxRpm * 1.1) / 1000.0) * 1000.0);
    if (!qFuzzyCompare(m_axisRpm->max(), rpmTop))
        m_axisRpm->setRange(0, rpmTop);
    m_axisOther->setRange(-20, 200);
}

void GraphWidget::onHover(const QPointF& value, bool inside)
{
    if (!inside || m_points.isEmpty()) {
        m_cursorLine->hide();
        return;
    }

    // Point le plus proche (recherche dichotomique, les temps sont croissants).
    const double t = value.x();
    auto it = std::lower_bound(m_points.cbegin(), m_points.cend(), t,
                               [](const Point& p, double x) { return p.t < x; });
    if (it == m_points.cend())
        --it;
    if (it != m_points.cbegin()) {
        auto prev = it - 1;
        if (qAbs(prev->t - t) < qAbs(it->t - t))
            it = prev;
    }

    const QPointF pos = m_chart->mapToPosition(QPointF(it->t, 0), m_rpm);
    const QRectF area = m_chart->plotArea();
    if (pos.x() < area.left() || pos.x() > area.right()) {
        m_cursorLine->hide();
        return;
    }
    m_cursorLine->setLine(pos.x(), area.top(), pos.x(), area.bottom());
    m_cursorLine->show();

    const VehicleSample& s = it->s;
    m_cursorLabel->setText(tr("%1  |  %2 tr/min  |  %3 km/h  |  %4 °C  |  papillon %5 %  |  charge %6 %")
                               .arg(s.timestamp.toString(QStringLiteral("HH:mm:ss")),
                                    fmt(s.rpm, 0), fmt(s.speed, 0), fmt(s.coolant, 0),
                                    fmt(s.throttle, 1), fmt(s.engineLoad, 1)));
}

QImage GraphWidget::capture(const QSize& size) const
{
    const bool cursorVisible = m_cursorLine->isVisible();
    m_cursorLine->hide();

    // La vue est rendue telle qu'affichée ; si l'onglet n'a jamais été montré,
    // on lui donne d'abord une taille raisonnable.
    if (m_view->width() < 200 || m_view->height() < 150)
        m_view->resize(size);

    const QImage image = m_view->grab().toImage();

    if (cursorVisible)
        m_cursorLine->show();
    return image;
}

void GraphWidget::applyTheme(bool dark)
{
    // Le thème QtCharts réinitialise les couleurs : on les réapplique ensuite.
    m_chart->setTheme(dark ? QChart::ChartThemeDark : QChart::ChartThemeLight);

    const QList<QPair<QLineSeries*, QColor>> colors = {
        { m_rpm, QColor(230, 70, 60) },  { m_speed, QColor(66, 133, 244) },
        { m_coolant, QColor(250, 160, 30) }, { m_throttle, QColor(60, 180, 90) },
        { m_load, QColor(170, 90, 210) },
    };
    for (const auto& c : colors) {
        QPen pen(c.second);
        pen.setWidthF(1.8);
        c.first->setPen(pen);
    }
    m_cursorLine->setPen(QPen(dark ? QColor(200, 200, 200) : QColor(110, 110, 110), 1, Qt::DashLine));
}
