#include "GaugeWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace
{
constexpr double kStartAngle = 225.0;   // degrés, sens trigonométrique
constexpr double kSpan = 270.0;
}

GaugeWidget::GaugeWidget(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_animation.setDuration(250);
    m_animation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_displayed = v.toDouble();
        update();
    });
}

void GaugeWidget::setTitle(const QString& title) { m_title = title; update(); }
void GaugeWidget::setUnit(const QString& unit)   { m_unit = unit; update(); }

void GaugeWidget::setRange(double min, double max)
{
    m_min = min;
    m_max = max > min ? max : min + 1;
    m_displayed = m_min;
    update();
}

void GaugeWidget::setZones(double warning, double danger, bool inverted)
{
    m_warning = warning;
    m_danger = danger;
    m_inverted = inverted;
    update();
}

void GaugeWidget::setValue(double value)
{
    m_value = value;

    const double target = qIsNaN(value) ? m_min : qBound(m_min, value, m_max);
    m_animation.stop();
    m_animation.setStartValue(m_displayed);
    m_animation.setEndValue(target);
    m_animation.start();
}

double GaugeWidget::ratio(double v) const
{
    return qBound(0.0, (v - m_min) / (m_max - m_min), 1.0);
}

void GaugeWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int side = qMin(width(), height());
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(side / 200.0, side / 200.0);   // repère logique : -100..100

    const QPalette pal = palette();
    const QColor fg = pal.color(QPalette::WindowText);
    const bool dark = pal.color(QPalette::Window).lightness() < 128;

    // Fond du cadran
    QRadialGradient bg(0, 0, 98);
    bg.setColorAt(0.0, dark ? QColor(48, 50, 56) : QColor(250, 250, 252));
    bg.setColorAt(1.0, dark ? QColor(24, 25, 28) : QColor(220, 223, 230));
    p.setPen(QPen(dark ? QColor(80, 82, 90) : QColor(170, 172, 180), 2));
    p.setBrush(bg);
    p.drawEllipse(QPointF(0, 0), 96, 96);

    const QRectF arcRect(-80, -80, 160, 160);

    // Arc de fond
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(dark ? QColor(70, 72, 80) : QColor(200, 202, 210), 8, Qt::SolidLine, Qt::FlatCap));
    p.drawArc(arcRect, int(kStartAngle * 16), int(-kSpan * 16));

    // Zones colorées
    auto drawZone = [&](double from, double to, const QColor& color) {
        const double a0 = kStartAngle - ratio(from) * kSpan;
        const double a1 = kStartAngle - ratio(to) * kSpan;
        p.setPen(QPen(color, 8, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(arcRect, int(a0 * 16), int((a1 - a0) * 16));
    };

    const QColor orange(240, 160, 30);
    const QColor red(220, 50, 50);
    if (!qIsNaN(m_warning) && !qIsNaN(m_danger)) {
        if (m_inverted) {
            drawZone(m_min, m_danger, red);
            drawZone(m_danger, m_warning, orange);
        } else {
            drawZone(m_warning, m_danger, orange);
            drawZone(m_danger, m_max, red);
        }
    }

    // Graduations
    p.setFont(QFont(font().family(), 8));
    const int minorPerMajor = 4;
    const int totalTicks = (m_majorTicks - 1) * minorPerMajor;
    for (int i = 0; i <= totalTicks; ++i) {
        const double r = double(i) / totalTicks;
        const double angle = qDegreesToRadians(kStartAngle - r * kSpan);
        const bool major = (i % minorPerMajor) == 0;
        const double rOut = 74;
        const double rIn = major ? 64 : 69;

        p.setPen(QPen(fg, major ? 2 : 1));
        p.drawLine(QPointF(rOut * qCos(angle), -rOut * qSin(angle)),
                   QPointF(rIn * qCos(angle), -rIn * qSin(angle)));

        if (major) {
            const double v = (m_min + r * (m_max - m_min)) / m_tickDivisor;
            const double step = (m_max - m_min) / m_tickDivisor / (m_majorTicks - 1);
            const QString label = QString::number(v, 'f', step >= 1.0 ? 0 : 1);
            const QPointF c(52 * qCos(angle), -52 * qSin(angle));
            p.drawText(QRectF(c.x() - 16, c.y() - 7, 32, 14), Qt::AlignCenter, label);
        }
    }

    // Titre
    p.setPen(fg);
    p.setFont(QFont(font().family(), 9, QFont::Bold));
    p.drawText(QRectF(-60, -38, 120, 16), Qt::AlignCenter, m_title);

    // Valeur numérique
    QColor valueColor = fg;
    if (!qIsNaN(m_value) && !qIsNaN(m_danger) && !qIsNaN(m_warning)) {
        const bool danger = m_inverted ? m_value <= m_danger : m_value >= m_danger;
        const bool warn = m_inverted ? m_value <= m_warning : m_value >= m_warning;
        valueColor = danger ? red : warn ? orange : fg;
    }
    p.setPen(valueColor);
    p.setFont(QFont(font().family(), 18, QFont::Bold));
    const QString valueText = qIsNaN(m_value) ? QStringLiteral("--")
                                              : QString::number(m_value, 'f', m_decimals);
    p.drawText(QRectF(-60, 28, 120, 26), Qt::AlignCenter, valueText);

    p.setPen(fg);
    p.setFont(QFont(font().family(), 8));
    p.drawText(QRectF(-60, 54, 120, 14), Qt::AlignCenter, m_unit);

    // Aiguille
    const double needleAngle = kStartAngle - ratio(m_displayed) * kSpan;
    p.save();
    p.rotate(-needleAngle);   // rotate() est horaire en coordonnées écran
    QPainterPath needle;
    needle.moveTo(-10, -3);
    needle.lineTo(70, 0);
    needle.lineTo(-10, 3);
    needle.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(230, 60, 50));
    p.drawPath(needle);
    p.restore();

    // Moyeu
    p.setPen(Qt::NoPen);
    p.setBrush(dark ? QColor(90, 92, 100) : QColor(60, 62, 70));
    p.drawEllipse(QPointF(0, 0), 7, 7);
}
