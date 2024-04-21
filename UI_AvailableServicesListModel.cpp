#include "UI_AvailableServicesListModel.h"

#include "ServiceDiscovery/DNSSDDiscoveryManager.h"

#include <QTimer>
#include <QPointer>

namespace UI
{

AvailableServicesListModel::AvailableServicesListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    //detectedItems_.push_back({12412, {{}, "214124"}});
    detectServices();
}

void AvailableServicesListModel::detectServices()
{
    auto weakThis = QPointer<AvailableServicesListModel>(this);

    using namespace std::placeholders;
    auto detectHandler = std::bind(AvailableServicesListModel::onServiceDetectionEvent, weakThis, _1, _2);

    using namespace DNSServiceDiscovery;
    DiscoveryManager::getInstance().detectService({"micBridge", TransportProtocol::UDP}, std::move(detectHandler));
}

void AvailableServicesListModel::onServiceDetectionEvent(QPointer<AvailableServicesListModel> weakThis,
                                                         DNSServiceDiscovery::StatusCode status,
                                                         DNSServiceDiscovery::DetectedServiceData data)
{
    using namespace DNSServiceDiscovery;
    if (status != StatusCode::OK || !weakThis)
    {
        return;
    }

    if (data.action == ServiceAction::Remove)
    {
        while(true)
        {
            auto& items = weakThis->detectedItems_;
            auto iter = std::find_if(items.begin(), items.end(), [&data](const auto& item)
                {
                    return data.id == item.first;
                });

            if (iter == items.end())
            {
                break;
            }

            const auto index = std::distance(items.begin(), iter);
            weakThis->beginRemoveRows(QModelIndex(), index, index);
            weakThis->detectedItems_.erase(iter);
            weakThis->endRemoveRows();
        }
    }
    else
    {
        using namespace std::placeholders;
        auto resolveHandler = std::bind(AvailableServicesListModel::onServiceResolveEvent, weakThis, _1, _2);
        DiscoveryManager::getInstance().resolveService(data, std::move(resolveHandler));
    }
}

void AvailableServicesListModel::onServiceResolveEvent(QPointer<AvailableServicesListModel> weakThis,
                                                       DNSServiceDiscovery::StatusCode status,
                                                       DNSServiceDiscovery::ResolvedServiceData data)
{
    using namespace DNSServiceDiscovery;
    if (status != StatusCode::OK || !weakThis)
    {
        return;
    }

    DiscoveryManager::getInstance()
        .queryServiceData(data, ServiceQueryType::IPv4,
        [weakThis, data](const auto status, auto id, std::string ipv4)
        {
            if (status != DNSServiceDiscovery::StatusCode::OK)
            {
                return;
            }

            if (weakThis)
            {
                auto& items = weakThis->detectedItems_;
                auto iter = std::find_if(items.begin(), items.end(),
                    [&id](const auto& item)
                    {
                        return id == item.second.data.id;
                    });

                if (iter != items.end())
                {
                    iter->second.data = std::move(data);
                    iter->second.ipv4 = std::move(ipv4);
                    const auto row = std::distance(items.begin(), iter);
                    emit weakThis->dataChanged(weakThis->index(row, 0), weakThis->index(row, 0));
                }
                else
                {
                    weakThis->beginInsertRows(QModelIndex(), items.size(), items.size());
                    items.push_back({id, {std::move(data), ipv4}});
                    weakThis->endInsertRows();
                }
            }
        });
}

int AvailableServicesListModel::rowCount(const QModelIndex&) const
{
    return detectedItems_.size();
}

int AvailableServicesListModel::columnCount(const QModelIndex&) const
{
    return 3;
}

QVariant AvailableServicesListModel::data(const QModelIndex &index, int role) const
{
    if (index.column() == 0)
    {
        auto iter = detectedItems_.begin();
        std::advance(iter, index.row());

        switch (role)
        {
        case Qt::DisplayRole:
            return QString::fromStdString(iter->second.data.name);
        case DataCode::IPv4:
            return QString::fromStdString(iter->second.ipv4);
        case DataCode::Port:
            return iter->second.data.port;
        }
    }

    return QVariant{};
}

} // namespace UI
