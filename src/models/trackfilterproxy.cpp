#include "trackfilterproxy.h"

TrackFilterProxy::TrackFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterRole(TrackListModel::SearchKeyRole);
    setDynamicSortFilter(true);
}

void TrackFilterProxy::setNeedle(const QString &normalizedNeedle)
{
    if (m_needle == normalizedNeedle) return;
    m_needle = normalizedNeedle;
    // Qt 6.10+: begin/endFilterChange avoids the deprecated invalidateFilter().
    beginFilterChange();
    endFilterChange();
}

bool TrackFilterProxy::filterAcceptsRow(int sourceRow,
                                        const QModelIndex &sourceParent) const
{
    if (m_needle.isEmpty()) return true;
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString key = idx.data(TrackListModel::SearchKeyRole).toString();
    return key.contains(m_needle);
}
