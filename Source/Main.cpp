#include "Frontend/Launcher.hpp"
#include "Frontend/MainWindow.hpp"
#include <QApplication>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <exception>

// Usage: gbemu [rom]
//
// gbemu runs as a copy of itself, detached from the terminal, and returns at
// once (see Launcher). Boot ROMs are chosen in the Settings window.
int main(int argc, char **argv)
{
    // Start the detached copy, with only Qt's core so this exits quickly. If
    // it can't be started, gbemu runs here instead.
    if (!Launcher::isDetachedCopy())
    {
        QCoreApplication launcher(argc, argv);
        if (Launcher::startDetachedCopy()) return 0;
    }

    QApplication application(argc, argv);
    application.setApplicationName("gbemu");
    application.setApplicationDisplayName("gbemu");
    // Errors the window doesn't catch still get a message before gbemu ends
    std::set_terminate(MainWindow::reportUncaughtException);

    // The first argument is the ROM
    const QStringList romArguments = QCoreApplication::arguments().mid(1);

    MainWindow window;
    window.show();
    if (!romArguments.isEmpty()) window.loadInitialROM(romArguments.first());
    return application.exec();
}
