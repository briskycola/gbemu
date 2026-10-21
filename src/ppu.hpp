#pragma once

#include "framebuffer.hpp"
#include "mmu.hpp"
#include "register.hpp"
#include "common.hpp"
#include <vector>
#include <functional>
#include <array>

static constexpr uint32_t CLOCKS_PER_HBLANK        = 204;  // Mode 0
static constexpr uint32_t CLOCKS_PER_VBLANK        = 4560; // Mode 1
static constexpr uint32_t CLOCKS_PER_SCANLINE_OAM  = 80;   // Mode 2
static constexpr uint32_t CLOCKS_PER_SCANLINE_VRAM = 172;  // Mode 3

static constexpr uint32_t CLOCKS_PER_SCANLINE =
    (CLOCKS_PER_SCANLINE_OAM + CLOCKS_PER_SCANLINE_VRAM + CLOCKS_PER_HBLANK);
static constexpr uint32_t SCANLINES_PER_FRAME = 144;
static constexpr uint32_t CLOCKS_PER_FRAME =
    (CLOCKS_PER_SCANLINE * SCANLINES_PER_FRAME) + CLOCKS_PER_VBLANK;

class Emulator;

enum class VideoMode
{
    ACCESS_OAM,
    ACCESS_VRAM,
    HBLANK,
    VBLANK,
};

class TileInfo
{
    public:
        uint8_t line;
        std::vector<uint8_t> pixels;
};

class PPU
{
    private:
        Emulator &emulator;

        FrameBuffer buffer;
        FrameBuffer backgroundMap;

        std::vector<uint8_t> videoRAM;

        VideoMode currentMode = VideoMode::ACCESS_OAM;
        uint32_t cycleCounter = 0;
        uint32_t screenEnableDelay = 0;
        uint8_t vramBank = 0;
        uint8_t objectPriorityMode = 0;
        std::array<uint8_t, 64> bgColorPalettes{};
        std::array<uint8_t, 64> objColorPalettes{};
        uint8_t bgPaletteIndex = 0;
        uint8_t objPaletteIndex = 0;
        std::array<uint8_t, GAMEBOY_WIDTH * GAMEBOY_HEIGHT> bgAttributePriority{};

        std::function<void(const FrameBuffer&)> vblankCallback;

        void writeScanline(uint8_t currentLine);
        void writeSprites();
        void draw();
        void drawBGLine(uint32_t currentLine);
        void drawWindowLine(uint32_t currentLine);
        void drawSprite(uint32_t sprite);
        GBColor getPixelFromLine(uint8_t byteOne, uint8_t byteTwo, uint8_t pixelIndex);

        bool isOnScreen(int32_t x, int32_t y);
        bool isOnScreenX(int32_t x);
        bool isOnScreenY(int32_t y);

        bool displayEnabled() const;
        bool windowTileMap() const;
        bool windowEnabled() const;
        bool bgWindowTileData() const;
        bool bgTileMapDisplay() const;
        bool spriteSize() const;
        bool spritesEnabled() const;
        bool bgEnabled() const;

        TileInfo getTileInfo(Address &tileSetLocation, uint8_t tileID, uint8_t tileLine) const;

        Color getRealColor(uint8_t pixelValue);
        colorPalette loadPalette(ByteRegister &paletteRegister);
        Color getColorFromPalette(GBColor color, const colorPalette &palette) const;
    public:
        PPU(Emulator &emulator);

        void tick(Cycles cycles);
        void setControlByte(uint8_t value);
        void registerVBlankCallback(const std::function<void(const FrameBuffer&)> &vblankCallback);

        uint8_t read(const Address &address);
        void write(const Address &address, uint8_t value);
        bool cpuCanAccessVRAM() const;
        bool cpuCanAccessOAM() const;
        void setVRAMBank(uint8_t bank) { vramBank = bank & 1; }
        uint8_t getVRAMBank() const { return vramBank; }
        void setObjectPriorityMode(uint8_t mode) { objectPriorityMode = mode & 1; }
        uint8_t getObjectPriorityMode() const { return objectPriorityMode; }
        uint8_t readCGBPaletteRegister(uint16_t address) const;
        void writeCGBPaletteRegister(uint16_t address, uint8_t value);
        uint8_t readVRAMBank(uint16_t offset, uint8_t bank) const;
        uint32_t getCGBColor(bool objectPalette, uint8_t palette, uint8_t color) const;

        uint8_t controlByte;

        ByteRegister lcdControl;
        ByteRegister lcdStatus;

        ByteRegister scrollX;
        ByteRegister scrollY;

        // LCDC Y-Coordinate
        ByteRegister line; // Line y-positon -> register LY
        ByteRegister lyCompare;

        ByteRegister windowX; // Note: x - 7
        ByteRegister windowY;

        ByteRegister bgPalette;
        ByteRegister spritePalette0; // OBP0
        ByteRegister spritePalette1; // OBP1
        
        // Unimplemented: LCD Color Palettes (CGB)
        // Unimplemented: LCD VRAM Bank (CGB)

        ByteRegister dmaTransfer;
};
