#include "UI_MainWindow.h"

#include "UI_AvailableServicesList.h"
#include "UI_AvailableServicesListModel.h"
#include "UI_AudioLevelsIODevice.h"
#include "UI_DriverControl.h"

#include "Broadcast/BC_Listener.h"

#include <QAudioFormat>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFile>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QMovie>
#include <QPainter>
#include <QStackedLayout>
#include <QTableView>
#include <QTimer>
#include <QThread>
#include <QRegExpValidator>
#include <QVBoxLayout>

#include <array>
#include <iostream>

using VoidEvent = Broadcast::DispatchQueue::VoidEvent;
Q_DECLARE_METATYPE(VoidEvent);

namespace UI
{

namespace
{
constexpr QSize DefaultSize(360, 240);
} // namespace

class MainWindow::EventsHandlerImpl : public Broadcast::ErrorHandler
                                    , public Broadcast::SuccessHandler
{
public:
    EventsHandlerImpl(MainWindow* parent)
     : parent_(parent)
    {
    }

    void onErrorOccured(int code, const std::string& errorMsg) override
    {
        constexpr int UnauthorizedError = 401;

        auto* errorDialog = new QMessageBox;
        errorDialog->setWindowFlags(errorDialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
        errorDialog->setAttribute(Qt::WA_DeleteOnClose);

        errorDialog->setIcon(QMessageBox::Icon::Critical);
        errorDialog->setDetailedText(QString::fromStdString(errorMsg));

        errorDialog->addButton(QMessageBox::Close);
        errorDialog->addButton(QMessageBox::Retry);

        if (code == UnauthorizedError)
        {
            connect(errorDialog, &QMessageBox::accepted, parent_, &MainWindow::showAuthCodeDialog);
            errorDialog->setWindowTitle("Authentification error");
            errorDialog->setText("Wrong authentification code, try again.");
        }
        else
        {
            connect(errorDialog, &QMessageBox::accepted, parent_, &MainWindow::doConnect);
            errorDialog->setWindowTitle("Connection error");
            errorDialog->setText("An error occured during connecting to the destination host.");
        }

        errorDialog->open();

        parent_->onConnectionRequestProcessed(false, "");
    }

    void onConnectSuccess(const std::string& ip) override
    {
        parent_->onConnectionRequestProcessed(true, ip);
    }

   private:
    QPointer<MainWindow> parent_;
};

class AudioFrameHandlerImpl : public Broadcast::AudioFramesHandler
{
 public:
  void onFrame(const std::uint8_t *, std::size_t len) override
  {
  }
};

class DispatchQueueImpl : public QObject, public Broadcast::DispatchQueue
{
  Q_OBJECT

public:
    void dispatchEvent(VoidEvent event) override
    {
        QMetaObject::invokeMethod(this, "perform", Qt::QueuedConnection, Q_ARG(VoidEvent, event));
    }

public slots:
    void perform(VoidEvent event)
    {
        Q_ASSERT(QThread::currentThread() == QObject::thread());
        event();
    }
};

class MainWindow::Loader : public QLabel
{
public:
    Loader(QWidget* parent = nullptr)
        : QLabel(parent)
    {
        QMovie *movie = new QMovie(":icons/general/loader");
        setMovie(movie);
        setStyleSheet("background: transparent;");
        setAlignment(Qt::AlignCenter);
    }

    void play()
    {
        show();
        movie()->start();
    }

    void stop()
    {
        hide();
        movie()->stop();
    }
};

class AudioLevelBar : public QProgressBar
{
public:
    AudioLevelBar(QWidget* parent)
        : QProgressBar(parent)
    {
        setValue(0);
        setTextVisible(false);
        setOrientation(Qt::Vertical);
    }

protected:
    void paintEvent(QPaintEvent *)
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        constexpr double ChunksCount = 20.;
        constexpr double SpacingBetweenChunks = 2.;

        const double chunkHeight = (height() - (ChunksCount - 1) * SpacingBetweenChunks) / ChunksCount;
        double y = height() - chunkHeight;

        int currentValue = value();
        for (int i = 0; i < ChunksCount; ++i)
        {
            if (currentValue > 0 && (currentValue * ChunksCount / 10) >= i * 10)
            {
                if (i < 13)
                {
                    p.setPen(QColor(51, 180, 70));
                    p.setBrush(QBrush(QColor(51, 180, 70)));
                }
                else if (i < 16)
                {
                    p.setPen(QColor(235, 231, 61));
                    p.setBrush(QBrush(QColor(235, 231, 61)));
                }
                else if (i < 18)
                {
                    p.setPen(QColor(255, 180, 70));
                    p.setBrush(QBrush(QColor(255, 180, 70)));
                }
                else if (i < 20)
                {
                    p.setPen(QColor(205, 21, 23));
                    p.setBrush(QBrush(QColor(205, 21, 23)));
                }
            }
            else
            {
                p.setPen(QColor(185, 215, 222));
                p.setBrush(QBrush(QColor(185, 215, 222)));
            }

            p.drawRect(QRectF(0, y, width(), chunkHeight));
            y -= chunkHeight + SpacingBetweenChunks;
        }
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , levels_(10)
{
    setWindowTitle("MicBridge Desktop");
    doLayout();
    resize(DefaultSize);
}

MainWindow::~MainWindow() = default;

void MainWindow::doLayout()
{
    auto* stack = new QFrame(this);
    auto* stackLayout = new QStackedLayout(stack);
    stackLayout->setStackingMode(QStackedLayout::StackAll);
    setCentralWidget(stack);
    stack->show();

    loader_ = new Loader(stack);
    stackLayout->addWidget(loader_);

    auto* centralWidget = new QFrame(this);
    auto* topLevelLayout = new QVBoxLayout(centralWidget);
    centralWidget->show();

    auto* mainWidget = new QWidget(centralWidget);
    mainWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    {
        auto* mainWidgetLayout = new QVBoxLayout(mainWidget);

        auto* deviceControls = new QHBoxLayout();
        servicesList_ = new AvailableServicesList(mainWidget);
        deviceControls->addWidget(servicesList_);

        connect(servicesList_, &AvailableServicesList::connectClicked, this,
            [this](std::string name, std::string ip, std::uint16_t port)
                {
                    activeDeviceName_ = name;
                    destinationIp_ = std::move(ip);
                    port_ = port;
                    showAuthCodeDialog();
                });

        connect(servicesList_, &AvailableServicesList::disconnectClicked, this,
            [this]()
            {
                listener_ = nullptr;
                activeDeviceName_.clear();
                destinationIp_.clear();
                port_ = 0;
                servicesList_->setCurrentDeviceDisconnected();
                updateStatusWidgets();
            });

        connect(servicesList_, &AvailableServicesList::connectedDeviceLost, this,
            [this]()
            {
                listener_ = nullptr;
                activeDeviceName_.clear();
                destinationIp_.clear();
                port_ = 0;
                updateStatusWidgets();
            });

        audioLevelBar_ = new AudioLevelBar(mainWidget);
        deviceControls->addWidget(audioLevelBar_);

        mainWidgetLayout->addLayout(deviceControls);
        mainWidgetLayout->addStretch();

        auto* statusWidget = new QWidget(mainWidget);
        auto* statusLayout = new QHBoxLayout(statusWidget);
        statusLayout->setContentsMargins({});

        statusLabel_ = new QLabel(mainWidget);
        statusLabel_->setStyleSheet(" font-size: 14px; color: rgba(39, 64, 75, 236); font-weight: 400;");
        statusLayout->addWidget(statusLabel_, 0, Qt::AlignLeft);

//        listenCheckbox_ = new QCheckBox(mainWidget);
//        listenCheckbox_->setStyleSheet(" font-size: 14px; color: rgba(39, 64, 75, 236); font-weight: 400;");
//        connect(listenCheckbox_, &QCheckBox::stateChanged, this, &MainWindow::onListenDeviceChecked);
//        statusLayout->addWidget(listenCheckbox_, 0, Qt::AlignLeft);

        mainWidgetLayout->addWidget(statusWidget);

        updateStatusWidgets();
    }

    topLevelLayout->addWidget(mainWidget);

    auto* bottomButtonsFrame = new QFrame(centralWidget);
    {
        auto*  buttonsLayout = new QHBoxLayout(bottomButtonsFrame);

        auto* manualConnectButton = new QPushButton("I can't find my device", bottomButtonsFrame);
        manualConnectButton->setStyleSheet("QPushButton"
                                           "{"
                                           "    font-size: 12px; "
                                           "    background: transparent; "
                                           "    color: rgb(24, 130, 220); "
                                           "}"
                                           "QPushButton:hover"
                                           "{"
                                           "    color: rgb(0, 160, 240); "
                                           "    text-decoration: underline; "
                                           "}");
        connect(manualConnectButton, &QPushButton::clicked, this, &MainWindow::showConnectDialog);
        buttonsLayout->addWidget(manualConnectButton);

        buttonsLayout->addStretch();

        auto* closeButton = new QPushButton("Close", bottomButtonsFrame);
        connect(closeButton, &QPushButton::clicked, this, &MainWindow::close);
        buttonsLayout->addWidget(closeButton);
    }

    bottomButtonsFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    topLevelLayout->addWidget(bottomButtonsFrame);
    stackLayout->addWidget(centralWidget);

    stackLayout->setCurrentIndex(1);

    setMaximumSize(532, 320);
}

QStackedLayout* MainWindow::getStackLayout()
{
    return static_cast<QStackedLayout*>(centralWidget()->layout());
}

void MainWindow::updateStatusWidgets()
{
    if (listener_)
    {
        if (!activeDeviceName_.empty())
        {
            statusLabel_->setText("Connected to the device: " + QString::fromStdString(activeDeviceName_));
        }
        else
        {
            statusLabel_->setText("Connected to the device with IP: " + QString::fromStdString(destinationIp_));
        }

        if (listenCheckbox_)
        {
            listenCheckbox_->setVisible(true);
        }
    }
    else
    {
        statusLabel_->setText("You are not connected to any device.");

        if (listenCheckbox_)
        {
            listenCheckbox_->setChecked(false);
            listenCheckbox_->setVisible(false);
        }
    }
}

void MainWindow::showAuthCodeDialog()
{
    auto* dialog = new QDialog(this);
    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog->setWindowTitle("Authentification code");

    auto* dialogLayout = new QVBoxLayout(dialog);
    auto* destinationLayout = new QHBoxLayout();

    //QString code = "[0-9a-zA-Z]";
    //QRegExp codeRegex("^" + code + "\\-" + code + "\\-" + code + "\\-" + code + "$");
    //QRegExpValidator* codeValidator = new QRegExpValidator(codeRegex, dialog);

    auto* codeEditor = new QLineEdit(dialog);
    //codeEditor->setValidator(codeValidator);
    codeEditor->setMaximumWidth(92);
    codeEditor->setInputMask(">N-N-N-N;");
    auto font = codeEditor->font();
    font.setPixelSize(16);
    codeEditor->setFont(font);
    destinationLayout->addWidget(codeEditor);

    dialogLayout->addLayout(destinationLayout);
    destinationLayout->setAlignment(Qt::AlignCenter);

    auto* dialogButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    connect(dialogButtons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(dialogButtons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    connect(dialog, &QDialog::accepted, codeEditor, [this, codeEditor]()
        {
            auto code = codeEditor->text();
            code.remove("-");
            authCode_ = code.toStdString();
            doConnect();
        });

    dialogLayout->addWidget(dialogButtons, 0, Qt::AlignRight | Qt::AlignBottom);

    dialog->resize(240, 120);

    dialog->exec();
}

void MainWindow::showConnectDialog()
{
    auto* dialog = new QDialog(this);
    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog->setWindowTitle("Connect to device");
    dialog->setStyleSheet("QLabel "
                          "{"
                          "    font-size: 14px; "
                          "color: rgba(39, 64, 75, 236); "
                          "font-weight: 400;"
                          "}");

    auto* dialogLayout = new QVBoxLayout(dialog);
    dialogLayout->setSpacing(0);

    auto* instructionLabel = new QLabel(dialog);
    instructionLabel->setText("<p style=\"line-height:115%\">"
                              "1. Go to iOS application and click <br>"
                              "   \"Don't know how to connect?\" link. <br>"
                              "2. Take a look at \"Manual connection data\" section <br>"
                              "   and input IP and PORT to the fields below:");

    dialogLayout->addWidget(instructionLabel);
    dialogLayout->addSpacing(12);

    auto* destinationLayout = new QHBoxLayout();
    destinationLayout->setSpacing(0);
    destinationLayout->setContentsMargins({});
    destinationLayout->addStretch();

    auto* ipLabel = new QLabel(dialog);
    ipLabel->setText("IP: ");
    destinationLayout->addWidget(ipLabel);

    QString ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
    QRegExp ipRegex ("^" + ipRange + "\\." + ipRange + "\\." + ipRange + "\\." + ipRange + "$");
    QRegExpValidator* ipValidator = new QRegExpValidator(ipRegex, dialog);

    auto* ipEditor = new QLineEdit(dialog);
    ipEditor->setValidator(ipValidator);
    ipEditor->setText("192.168.");
    ipEditor->setMaximumWidth(92);
    destinationLayout->addWidget(ipEditor);

    destinationLayout->addSpacing(32);

    auto* portLabel = new QLabel(dialog);
    portLabel->setText("PORT: ");
    destinationLayout->addWidget(portLabel);

    QString portRange = "^([1-9][0-9]{0,3}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$";
    QRegExp portRegex (portRange);
    QRegExpValidator* portValidator = new QRegExpValidator(portRegex, dialog);
    auto* portEditor = new QLineEdit(dialog);
    portEditor->setValidator(portValidator);
    portEditor->setMaximumWidth(48);
    destinationLayout->addWidget(portEditor);
    destinationLayout->addStretch();

    dialogLayout->addLayout(destinationLayout);
    dialogLayout->addSpacing(16);
    destinationLayout->setAlignment(Qt::AlignCenter);

    auto* dialogButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    connect(dialogButtons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(dialogButtons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    connect(dialog, &QDialog::accepted, ipEditor, [this, ipEditor, portEditor]()
        {
            destinationIp_ = ipEditor->text().toStdString();
            port_ = portEditor->text().toUInt();
            showAuthCodeDialog();
        });

    auto* hopelessLabel = new QLabel(dialog);
    hopelessLabel->setStyleSheet("font-size: 12px; ");
    hopelessLabel->setText("<p style=\"line-height:115%\">"
                           "If you still unable to connect to your device<br>"
                           "make sure PC and this device are connected to the same network.");

    dialogLayout->addWidget(hopelessLabel);
    dialogLayout->addSpacing(24);

    dialogLayout->addWidget(dialogButtons, 0, Qt::AlignRight | Qt::AlignBottom);

    dialog->setFixedSize(dialogLayout->sizeHint());

    dialog->exec();
}

void MainWindow::doConnect()
{
    getStackLayout()->setCurrentIndex(0);
    loader_->play();
    setEnabled(false);

    auto driverControl = std::make_shared<DriverControlFramesSender>();
    framesSender_ = driverControl;

    connect(driverControl->getAudioInfoIODevice(), &AudioInfo::update, this, &MainWindow::refreshDisplay);

    auto eventsHandler = std::make_shared<EventsHandlerImpl>(this);

    listener_ = std::make_unique<Broadcast::Listener>(destinationIp_,
                                                      port_,
                                                      authCode_,
                                                      std::move(driverControl),
                                                      std::make_shared<DispatchQueueImpl>(),
                                                      eventsHandler,
                                                      eventsHandler);
}

void MainWindow::onConnectionRequestProcessed(bool success, const std::string& ip)
{
    getStackLayout()->setCurrentIndex(1);
    loader_->stop();
    setEnabled(true);

    if (!success)
    {
        listener_ = nullptr;
    }
    else
    {
        servicesList_->setDeviceIsConnected(activeDeviceName_, destinationIp_, port_);
    }

    updateStatusWidgets();
}

void MainWindow::onListenDeviceChecked(bool isChecked)
{
    if (!isChecked)
    {
        listenOutput_->stop();
        listenOutput_->reset();
        listenOutput_ = nullptr;
        return;
    }

    auto input = framesSender_.lock();
    if (!input)
        return;

    input->getAudioInfoIODevice()->open(QIODevice::ReadOnly);
    const auto srcFormat = input->getAudioInfoIODevice()->getFormat();
    QAudioFormat format;
    format.setSampleRate(srcFormat.sampleRate);
    format.setChannelCount(srcFormat.channelsCount);
    format.setSampleSize(srcFormat.sampleLength);
    format.setCodec("audio/pcm");
    format.setByteOrder(srcFormat.isLittleEndian ? QAudioFormat::LittleEndian : QAudioFormat::BigEndian);
    format.setSampleType(srcFormat.isSigned ? QAudioFormat::SignedInt : QAudioFormat::UnSignedInt);

    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    if (!info.isFormatSupported(format)) {
        std::cerr << "Raw audio format not supported by backend, cannot play audio.";
        return;
    }

    listenOutput_ = new QAudioOutput(format, this);
    listenOutput_->start(input->getAudioInfoIODevice());
}

void MainWindow::refreshDisplay(qreal level)
{
  levels_[lastLevel_++] = level;
  if (lastLevel_ == levels_.size())
  {
    lastLevel_ = 0;
  }

  qreal sum = 0.0;
  for (auto i = 0u; i < levels_.size(); i++){
    sum += levels_[i];
  }

  sum = sum / levels_.size();

  auto newValue = sum * 100;
  audioLevelBar_->setValue(newValue);
}

} // namespace UI

#include "UI_MainWindow.moc"
