#pragma once

#include "cpu.hpp"
#include "cartridge.hpp"
#include "mmu.hpp"
#include "timer.hpp"
#include "serial.hpp"
#include "input.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include <memory>

class Emulator
{
    private:
        std::shared_ptr<Cartridge> cartridge;
        std::function<bool()> shouldCloseCallback;
        uint32_t elapsedCycles = 0;
        uint16_t dmaCyclesRemaining = 0;

        friend class CPU;
        CPU cpu;

        friend class MMU;
        MMU mmu;

        friend class Timer;
        Timer timer;

        friend class Serial;
        Serial serial;

        friend class Input;
        Input input;

        friend class PPU;
        PPU ppu;

        friend class MMU;
        APU apu;

        void tick();
    public:
        Emulator(const std::vector<uint8_t> &cartridgeData, const std::vector<uint8_t> &saveData = {});
        const std::vector<uint8_t>& getCartridgeRAM();
        std::vector<int16_t> takeAudioSamples();
        void run(std::function<bool()> shouldCloseCallback, std::function<void(const FrameBuffer&)> vblankCallback);

        void buttonPressed(GBButton button);
        void buttonReleased(GBButton button);
};
