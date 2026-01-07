#pragma once

#include "cpu.hpp"
#include "cartridge.hpp"
#include "mmu.hpp"
#include "timer.hpp"
#include <memory>

class Emulator
{
    private:
        std::shared_ptr<Cartridge> cartridge;

        friend class CPU;
        CPU cpu;

        friend class MMU;
        MMU mmu;

        friend class Timer;
        Timer timer;

        uint32_t elapsedCycles = 0;
        void tick();
    public:
        Emulator(const std::vector<uint8_t> &cartridgeData, const std::vector<uint8_t> &saveData = {});
        const std::vector<uint8_t>& getCartridgeRAM();
        void run();
};
