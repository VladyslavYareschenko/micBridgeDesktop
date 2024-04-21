#pragma once

#include <QAbstractItemModel>

#include "ServiceDiscovery/DNSSDDefs.h"

#include <vector>

#include <QAbstractTableModel>

namespace UI
{

class AvailableServicesListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    AvailableServicesListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    enum DataCode
    {
        IPv4 = Qt::UserRole,
        Port
    };

    QVariant data(const QModelIndex &index, int role) const override;

private:
    void detectServices();

    static void onServiceDetectionEvent(QPointer<AvailableServicesListModel> weakThis,
                                        DNSServiceDiscovery::StatusCode status,
                                        DNSServiceDiscovery::DetectedServiceData data);

    static void onServiceResolveEvent(QPointer<AvailableServicesListModel>,
                                      DNSServiceDiscovery::StatusCode status,
                                      DNSServiceDiscovery::ResolvedServiceData data);

private:
    struct ServiceItem
    {
        DNSServiceDiscovery::ResolvedServiceData data;
        std::string ipv4;
    };

    std::vector<std::pair<std::size_t, ServiceItem>> detectedItems_;
};

} // namespace UI
