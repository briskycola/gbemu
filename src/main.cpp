#include <iostream>
#include <memory>
#include "emulator.hpp"
#include "file.hpp"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <rom-file>\n";
        return 1;
    }
    std::unique_ptr<Emulator> emulator;
    std::vector<uint8_t> romData = readFile(argv[1]);
    emulator = std::make_unique<Emulator>(romData);
    emulator->run();
    return 0;
}
