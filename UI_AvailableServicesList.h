#pragma once

#include <QTableView>

class QPushButton;

namespace UI
{
class AvailableServicesListModel;

class AvailableServicesList : public QTableView
{
    Q_OBJECT
public:
    AvailableServicesList(QWidget* parent);

    void setDeviceIsConnected(std::string name, std::string ip, std::uint16_t port);
    void setCurrentDeviceDisconnected();

signals:
    void connectedDeviceLost();
    void connectClicked(std::string name, std::string ip, std::uint16_t port);
    void disconnectClicked(std::string name, std::string ip, std::uint16_t port);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void buttonHandler(QPushButton* button, bool connect);

private:
    AvailableServicesListModel* model_;

    std::string connectedServiceIp_;
    std::uint16_t connectedServicePort_ = 0;
};
} // namespace UI
