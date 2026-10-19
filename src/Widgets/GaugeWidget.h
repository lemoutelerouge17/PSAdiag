#pragma once

#include <QVariantAnimation>
#include <QtNumeric>
#include <QWidget>

// Jauge circulaire type compteur automobile (270°), avec zones colorées,
// graduations et aiguille animée.
class GaugeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GaugeWidget(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setRange(double min, double max);
    void setUnit(const QString& unit);
    void setDecimals(int decimals) { m_decimals = decimals; update(); }
    void setMajorTicks(int count) { m_majorTicks = qMax(2, count); update(); }
    void setTickDivisor(double divisor) { m_tickDivisor = divisor; update(); }

    // Zones : au-delà de warning → orange, au-delà de danger → rouge.
    // inverted = true : zones colorées sous les seuils (ex. tension basse).
    void setZones(double warning, double danger, bool inverted = false);

    void setValue(double value);
    double value() const { return m_value; }

    QSize sizeHint() const override { return { 260, 260 }; }
    QSize minimumSizeHint() const override { return { 160, 160 }; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double ratio(double v) const;

    QString m_title;
    QString m_unit;
    double m_min = 0;
    double m_max = 100;
    double m_value = qQNaN();
    double m_displayed = 0;
    int m_decimals = 0;
    int m_majorTicks = 9;
    double m_tickDivisor = 1;
    double m_warning = qQNaN();
    double m_danger = qQNaN();
    bool m_inverted = false;

    QVariantAnimation m_animation;
};
