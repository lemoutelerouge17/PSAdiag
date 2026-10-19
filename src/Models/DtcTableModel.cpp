#include "DtcTableModel.h"

#include <QBrush>
#include <QColor>

DtcTableModel::DtcTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void DtcTableModel::setDtcs(const QList<DTC>& dtcs)
{
    beginResetModel();
    m_dtcs = dtcs;
    endResetModel();
}

int DtcTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_dtcs.size());
}

int DtcTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant DtcTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_dtcs.size())
        return {};

    const DTC& dtc = m_dtcs.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case CodeColumn:        return dtc.code;
        case DescriptionColumn: return dtc.description;
        case StatusColumn:      return DTC::statusText(dtc.status);
        case SeverityColumn:
            return dtc.severity >= 3 ? tr("Critique")
                 : dtc.severity == 2 ? tr("Attention")
                                     : tr("Information");
        default: break;
        }
    }

    if (role == Qt::ForegroundRole && index.column() == SeverityColumn) {
        if (dtc.severity >= 3) return QBrush(QColor(220, 50, 50));
        if (dtc.severity == 2) return QBrush(QColor(230, 150, 30));
    }

    if (role == Qt::TextAlignmentRole && index.column() != DescriptionColumn)
        return int(Qt::AlignCenter);

    return {};
}

QVariant DtcTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};

    switch (section) {
    case CodeColumn:        return tr("Code");
    case DescriptionColumn: return tr("Description");
    case StatusColumn:      return tr("État");
    case SeverityColumn:    return tr("Gravité");
    default:                return {};
    }
}
