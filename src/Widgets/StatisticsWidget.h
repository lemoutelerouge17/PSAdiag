#pragma once

#include "Models/VehicleSample.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QTableWidget;

class StatisticsWidget : public QWidget
{
    Q_OBJECT

public:
    enum class Source { LiveSession, HistorySession };

    explicit StatisticsWidget(QWidget* parent = nullptr);

    void setSamples(const QList<VehicleSample>& samples, const QString& label);
    Source source() const;

signals:
    void refreshRequested(StatisticsWidget::Source source);

private:
    QComboBox* m_source;
    QLabel* m_label;
    QTableWidget* m_table;
};
