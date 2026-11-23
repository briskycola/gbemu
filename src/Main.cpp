#include "MainWindow.hpp"
#include <QApplication>
#include <QString>

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

    MainWindow window;
    window.show();
    if (argc > arguments::ROM_ARGUMENT)
        window.loadInitialROM(QString::fromLocal8Bit(argv[arguments::ROM_ARGUMENT]));
    return application.exec();
}
