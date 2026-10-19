#pragma once

#include "DTC/DTC.h"

#include <QWidget>

class DtcTableModel;
class QLabel;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QTableView;

class DTCWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DTCWidget(QWidget* parent = nullptr);

    void setDtcs(const QList<DTC>& dtcs, const QString& origin = QString());
    QList<DTC> dtcs() const;
    void setAdapterReady(bool ready);

signals:
    void readRequested();
    void clearRequested();
    void exportRequested();

private:
    void confirmClear();

    DtcTableModel* m_model;
    QSortFilterProxyModel* m_proxy;
    QTableView* m_table;
    QLineEdit* m_search;
    QLabel* m_summary;
    QPushButton* m_readButton;
    QPushButton* m_clearButton;
    QPushButton* m_exportButton;
};
