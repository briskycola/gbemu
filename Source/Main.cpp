#include "Frontend/MainWindow.hpp"
#include <QApplication>
#include <QString>
#include <exception>

// Usage: gbemu [rom]
// Boot ROMs are chosen in the Settings window.
namespace arguments
{
    static constexpr int ROM_ARGUMENT = 1;
}

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setApplicationName("gbemu");
    application.setApplicationDisplayName("gbemu");
    // Errors the window doesn't catch still get a message before gbemu ends
    std::set_terminate(MainWindow::reportUncaughtException);

    MainWindow window;
    window.show();
    if (argc > arguments::ROM_ARGUMENT)
        window.loadInitialROM(QString::fromLocal8Bit(argv[arguments::ROM_ARGUMENT]));
    return application.exec();
}
