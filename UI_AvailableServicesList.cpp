#include "UI_AvailableServicesList.h"

#include "UI_AvailableServicesListModel.h"

#include <iostream>

#include <QHeaderView>
#include <QPushButton>
#include <QPainter>

namespace UI
{

AvailableServicesList::AvailableServicesList(QWidget* parent)
    : QTableView(parent)
{
    model_ = new AvailableServicesListModel(this);

    setStyleSheet("QTableView "
                  "{"
                  "    color: rgba(20, 36, 42, 255);"
                  "    font-size: 14px; "
                  "    alternate-background-color: rgba(182, 194, 195, 48);"
                  "    border: 1px solid rgb(163, 171, 182);"
                  "}"
                  ""
                  "QTableView::item"
                  "{"
                  "    padding-left: 4px;"
                  "    padding-top: 2px;"
                  "    padding-bottom: 2px;"
                  "    padding-right: 4px;"
                  "    border-bottom: 1px solid rgba(163, 171, 182, 32);"
                  "}");

    setFocusPolicy(Qt::NoFocus);
    setEditTriggers(QTableView::NoEditTriggers);
    setSelectionMode(QTableView::NoSelection);

    setShowGrid(false);

    setAlternatingRowColors(true);

    horizontalHeader()->hide();
    verticalHeader()->hide();
    setModel(model_);

//     setColumnHidden(2, true);

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    auto onRowsChange = [this]()
    {
        bool hasConnectedDevice = false;

        for (int i = 0; i < model_->rowCount(); ++i)
        {
            auto* connectButton = new QPushButton("Connect");
            connectButton->setProperty("rowIndex", i);
            connectButton->setContentsMargins(0, 8, 0, 8);
            connect(connectButton,
                &QPushButton::clicked,
                this,
                [this, connectButton]()
                {
                    buttonHandler(connectButton, true);
                });

            auto* disconnectButton = new QPushButton("Disconnect");
            disconnectButton->setProperty("rowIndex", i);
            connect(disconnectButton,
                &QPushButton::clicked,
                this,
                [this, disconnectButton]()
                {
                    buttonHandler(disconnectButton, false);
                });

            auto item = model_->index(i, 0);
            const auto itemIP = item.data(AvailableServicesListModel::DataCode::IPv4).toString().toStdString();
            const auto itemPort = item.data(AvailableServicesListModel::DataCode::Port).toUInt();
            disconnectButton->setEnabled(itemIP == connectedServiceIp_ && itemPort == connectedServicePort_);

            if (!hasConnectedDevice)
            {
                hasConnectedDevice = itemIP == connectedServiceIp_ && itemPort == connectedServicePort_;
            }

            setIndexWidget(model_->index(i, 1), connectButton);
            setIndexWidget(model_->index(i, 2), disconnectButton);
        }

        if (!hasConnectedDevice && !connectedServiceIp_.empty() && connectedServicePort_ != 0)
        {
            setCurrentDeviceDisconnected();
            emit connectedDeviceLost();
        }
    };

    onRowsChange();
    connect(model_, &AvailableServicesListModel::rowsInserted, this, onRowsChange);
    connect(model_, &AvailableServicesListModel::rowsRemoved, this, onRowsChange);
}

void AvailableServicesList::setDeviceIsConnected(std::string, std::string ip, std::uint16_t port)
{
    connectedServiceIp_ = std::move(ip);
    connectedServicePort_ = port;

   for (int i = 0; i < model_->rowCount(); ++i)
   {
       auto item = model_->index(i, 0);
       const auto itemIP = item.data(AvailableServicesListModel::DataCode::IPv4).toString().toStdString();
       const auto itemPort = item.data(AvailableServicesListModel::DataCode::Port).toUInt();
       indexWidget(model_->index(i, 2))->setEnabled(itemIP == connectedServiceIp_ && itemPort == connectedServicePort_);
   }
}

void AvailableServicesList::setCurrentDeviceDisconnected()
{
    connectedServiceIp_.clear();
    connectedServicePort_ = 0;

    for (int i = 0; i < model_->rowCount(); ++i)
    {
        indexWidget(model_->index(i, 2))->setEnabled(false);
    }
}

void AvailableServicesList::buttonHandler(QPushButton* button, bool connect)
{
    auto row = button->property("rowIndex").toInt();
    auto item = model_->index(row, 0);

    const auto name = item.data(Qt::DisplayRole).toString().toStdString();

    const auto ipv4 = item.data(AvailableServicesListModel::DataCode::IPv4).toString().toStdString();

    bool isOk = false;
    const auto port = item.data(AvailableServicesListModel::DataCode::Port).toUInt(&isOk);

    if (!ipv4.empty() && isOk)
    {
        connect ? emit connectClicked(name, std::move(ipv4), port)
                : emit disconnectClicked(name, std::move(ipv4), port);
    }
}

void AvailableServicesList::paintEvent(QPaintEvent* event)
{
    {
        QColor backgroundColor = model_->rowCount() == 0 ? QColor(37, 56, 75, 32)
                                                         : QColor(37, 56, 75, 8);
        QPainter painter(viewport());
        painter.setPen(Qt::PenStyle::NoPen);
        painter.setBrush(QBrush(backgroundColor));
        painter.drawRect(0, 0, width(), height());
    }

    QTableView::paintEvent(event);

    if (model_->rowCount() == 0)
    {
        QPainter painter(viewport());
        painter.setPen(QColor(39, 64, 75, 236));
        auto font = painter.font();
        font.setBold(true);
        font.setPixelSize(14);
        painter.setFont(font);
        painter.drawText(QRectF(0, 0, width(), height()), Qt::AlignCenter, "No devices found.");
    }
}

} // namespace UI
