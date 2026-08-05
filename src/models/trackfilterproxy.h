#ifndef LUMEN_TRACKFILTERPROXY_H
#define LUMEN_TRACKFILTERPROXY_H

#include <QSortFilterProxyModel>
#include <QString>
#include "tracklistmodel.h"

class TrackFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit TrackFilterProxy(QObject *parent = nullptr);

    void setNeedle(const QString &normalizedNeedle);
    QString needle() const { return m_needle; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_needle;
};

#endif // LUMEN_TRACKFILTERPROXY_H
