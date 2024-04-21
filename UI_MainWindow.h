#pragma once

#include <QAudioOutput>
#include <QCheckBox>
#include <QMainWindow>
#include <QPointer>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

class QStackedLayout;

namespace Broadcast
{
class Listener;
} // namespace Broadcast

class AudioInfo;
class DriverControlFramesSender;

namespace UI
{

class AvailableServicesList;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    virtual ~MainWindow();

signals:

private:
    void doLayout();
    void updateStatusWidgets();

    void showAuthCodeDialog();
    void showConnectDialog();
    void doConnect();

    void readMore();
    void initializeAudio();
    void refreshDisplay(qreal level);

    void onConnectionRequestProcessed(bool success, const std::string& ip);
    void onListenDeviceChecked(bool isChecked);

    QStackedLayout* getStackLayout();

private:
    QPointer<AvailableServicesList> servicesList_;
    QPointer<QProgressBar> audioLevelBar_;
    QPointer<QLabel> statusLabel_;

    QPointer<QCheckBox> listenCheckbox_;
    QPointer<QAudioOutput> listenOutput_;

    class Loader;
    Loader* loader_;

    std::string destinationIp_;
    std::string activeDeviceName_;
    std::string authCode_;
    std::uint16_t port_ = 0;
    std::unique_ptr<Broadcast::Listener> listener_;
    std::weak_ptr<DriverControlFramesSender> framesSender_;

    std::vector<double> levels_;
    std::size_t lastLevel_ = 0;

    class EventsHandlerImpl;
};

} // namespace UI
