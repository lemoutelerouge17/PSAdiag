#pragma once

#include "Models/Vehicle.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

class VehicleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VehicleDialog(const Vehicle& vehicle, QWidget* parent = nullptr);

    Vehicle vehicle() const;

    static bool isValidVin(const QString& vin, QString* reason = nullptr);

    void accept() override;

private:
    Vehicle m_vehicle;

    QLineEdit* m_vin;
    QLineEdit* m_brand;
    QLineEdit* m_model;
    QLineEdit* m_engine;
    QLineEdit* m_registration;
    QSpinBox* m_year;
    QComboBox* m_fuel;
    QSpinBox* m_mileage;
    QPlainTextEdit* m_comments;
    QLabel* m_vinInfo;
};
