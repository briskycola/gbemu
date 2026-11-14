#include "File.hpp"
#include "MainWindow.hpp"
#include <QApplication>
#include <QString>
#include <cstdint>
#include <utility>
#include <vector>

// Positions of the command-line arguments: gbemu [rom] [cgb_boot_rom]
namespace arguments
{
    static constexpr int ROM_ARGUMENT      = 1;
    static constexpr int BOOT_ROM_ARGUMENT = 2;
}

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setApplicationName("gbemu");
    application.setApplicationDisplayName("gbemu");

    std::vector<uint8_t> bootROM;
    if (argc > arguments::BOOT_ROM_ARGUMENT) bootROM = readFile(argv[arguments::BOOT_ROM_ARGUMENT]);

    MainWindow window(std::move(bootROM));
    window.show();
    if (argc > arguments::ROM_ARGUMENT)
        window.loadInitialROM(QString::fromLocal8Bit(argv[arguments::ROM_ARGUMENT]));
    return application.exec();
}
