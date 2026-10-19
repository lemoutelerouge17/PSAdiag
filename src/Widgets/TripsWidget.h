#pragma once

#include "Models/Trip.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;

class TripsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TripsWidget(QWidget* parent = nullptr);

    void setTrips(const QList<Trip>& trips);
    QList<Trip> trips() const { return m_trips; }
    void setRunning(bool running);
    void setCanStart(bool canStart);
    void setLiveTrip(const Trip& trip, qint64 durationSec);

    static QString formatDuration(qint64 seconds);

signals:
    void startRequested();
    void stopRequested();
    void exportRequested();
    void deleteRequested(int tripId);

private:
    QList<Trip> m_trips;
    bool m_running = false;
    QPushButton* m_startStop;
    QPushButton* m_delete;
    QLabel* m_live;
    QTableWidget* m_table;
};
