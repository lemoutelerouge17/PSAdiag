#pragma once

#include "Models/VehicleSample.h"

#include <QWidget>

class GaugeWidget;
class QLabel;

class DashboardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardWidget(QWidget* parent = nullptr);

    void updateSample(const VehicleSample& sample);
    void clearValues();

    void setVin(const QString& vin);
    void setVehicleName(const QString& name);
    void setDtcCount(int count);
    void setConnectionState(const QString& text, bool ok);
    void setTripInfo(const QString& text);
    void setRecording(bool recording);

private:
    QLabel* makeValueLabel();

    GaugeWidget* m_rpmGauge;
    GaugeWidget* m_speedGauge;
    GaugeWidget* m_tempGauge;
    GaugeWidget* m_throttleGauge;

    QLabel* m_voltage;
    QLabel* m_load;
    QLabel* m_intake;
    QLabel* m_maf;
    QLabel* m_fuelPressure;

    QLabel* m_vin;
    QLabel* m_vehicle;
    QLabel* m_dtcCount;
    QLabel* m_connection;
    QLabel* m_trip;
    QLabel* m_recording;
};
