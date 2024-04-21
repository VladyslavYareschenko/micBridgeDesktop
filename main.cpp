#include "UI_MainWindow.h"

#include <QApplication>
#include <QFile>

QString loadStyleSheet(const QString& name)
{
    QFile file(name);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QString result = file.readAll();

    return result;
}

int main(int argc, char** argv)
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QApplication::setAttribute(Qt::AA_DisableHighDpiScaling, false);
    QApplication app(argc, argv);
    app.setStyleSheet(loadStyleSheet(":stylesheets/ApplicationStyle.css"));

    UI::MainWindow mainWindow;
    mainWindow.show();

    app.exec();
}
