#pragma once

#include "DTC/DTC.h"

#include <QAbstractTableModel>

class DtcTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { CodeColumn, DescriptionColumn, StatusColumn, SeverityColumn, ColumnCount };

    explicit DtcTableModel(QObject* parent = nullptr);

    void setDtcs(const QList<DTC>& dtcs);
    const QList<DTC>& dtcs() const { return m_dtcs; }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QList<DTC> m_dtcs;
};
