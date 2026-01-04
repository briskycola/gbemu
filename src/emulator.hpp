#pragma once

#include "cpu.hpp"
#include "cartridge.hpp"
#include <memory>

class Emulator
{
    private:
        std::shared_ptr<Cartridge> cartridge;

        friend class CPU;
        CPU cpu;

        uint32_t elapsedCycles = 0;
        void tick();
    public:
        Emulator(const std::vector<uint8_t> &cartridgeData, const std::vector<uint8_t> &saveData = {});
        std::vector<uint8_t>& getCartridgeRAM() const;
};
