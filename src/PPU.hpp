#pragma once

#include "FrameBuffer.hpp"
#include "MMU.hpp"
#include "Register.hpp"
#include "Common.hpp"
#include "Color.hpp"
#include <vector>
#include <functional>
#include <array>

static constexpr uint32_t CLOCKS_PER_HBLANK       = 204;  // Mode 0
static constexpr uint32_t CLOCKS_PER_VBLANK       = 4560; // Mode 1
static constexpr uint32_t CLOCKS_PER_SCANLINE_OAM  = 80;   // Mode 2
static constexpr uint32_t CLOCKS_PER_SCANLINE_VRAM = 172;  // Mode 3

static constexpr uint32_t CLOCKS_PER_SCANLINE =
    (CLOCKS_PER_SCANLINE_OAM + CLOCKS_PER_SCANLINE_VRAM + CLOCKS_PER_HBLANK);
static constexpr uint32_t SCANLINES_PER_FRAME = 144;
static constexpr uint32_t CLOCKS_PER_FRAME =
    (CLOCKS_PER_SCANLINE * SCANLINES_PER_FRAME) + CLOCKS_PER_VBLANK;

// LY counts lines 0-153. Lines 144-153 are the
// 10 invisible lines that make up VBlank.
static constexpr uint32_t SCANLINES_PER_FRAME_WITH_VBLANK = 154;

// Clocks between switching the LCD on and the
// first line starting to draw
static constexpr uint32_t LCD_ENABLE_DELAY = 244;

// LCDC (LCD control register) bits
static constexpr uint8_t LCDC_DISPLAY_ENABLE_BIT    = 7;
static constexpr uint8_t LCDC_WINDOW_TILE_MAP_BIT    = 6;
static constexpr uint8_t LCDC_WINDOW_ENABLE_BIT     = 5;
static constexpr uint8_t LCDC_BG_WINDOW_TILE_DATA_BIT = 4;
static constexpr uint8_t LCDC_BG_TILE_MAP_BIT        = 3;
static constexpr uint8_t LCDC_SPRITE_SIZE_BIT       = 2;
static constexpr uint8_t LCDC_SPRITES_ENABLE_BIT    = 1;
static constexpr uint8_t LCDC_BG_ENABLE_BIT         = 0; // BG/window priority on the CGB

// STAT (LCD status register) bits. Bits 0-1 hold the current
// mode and bit 2 the LY == LYC flag; both are read-only.
// Bits 3-6 enable the STAT interrupt sources and bit 7 is unused.
static constexpr uint8_t STAT_MODE_MASK            = 0x03;
static constexpr uint8_t STAT_COINCIDENCE_BIT      = 2;
static constexpr uint8_t STAT_HBLANK_INTERRUPT_BIT  = 3;
static constexpr uint8_t STAT_LYC_INTERRUPT_BIT     = 6;
static constexpr uint8_t STAT_READ_ONLY_MASK        = 0x07;
static constexpr uint8_t STAT_INTERRUPT_ENABLE_MASK = 0x78;
static constexpr uint8_t STAT_UNUSED_BITS          = 0x80;

// Sprites (objects). OAM holds 40 entries of 4 bytes:
// Y position, X position, tile number, and attributes.
static constexpr uint32_t MAX_SPRITES             = 40;
static constexpr uint16_t OAM_SPRITE_Y_BYTE         = 0;
static constexpr uint16_t OAM_SPRITE_X_BYTE         = 1;
static constexpr uint16_t OAM_SPRITE_TILE_BYTE      = 2;
static constexpr uint16_t OAM_SPRITE_ATTRIBUTE_BYTE = 3;

// Sprite positions are stored offset so that a sprite
// can be partially off the top or left of the screen.
// Y = 16 and X = 8 put a sprite at the top-left corner.
static constexpr int32_t SPRITE_Y_OFFSET = 16;
static constexpr int32_t SPRITE_X_OFFSET = 8;

// WX = 7 puts the window at the left edge of the screen
static constexpr uint32_t WINDOW_X_OFFSET = 7;

// Attribute bits for sprites (OAM byte 3) and, on the Game
// Boy Color, for background tiles (VRAM bank 1 tile map)
static constexpr uint8_t ATTRIBUTE_CGB_PALETTE_MASK = 0x07;
static constexpr uint8_t ATTRIBUTE_VRAM_BANK_BIT    = 3;
static constexpr uint8_t ATTRIBUTE_DMG_PALETTE_BIT  = 4;
static constexpr uint8_t ATTRIBUTE_X_FLIP_BIT       = 5;
static constexpr uint8_t ATTRIBUTE_Y_FLIP_BIT       = 6;
static constexpr uint8_t ATTRIBUTE_PRIORITY_BIT    = 7;

class Emulator;

// The values match the mode number reported in STAT bits 0-1
enum class VideoMode : uint8_t
{
    HBlank = 0,
    VBlank = 1,
    AccessOAM = 2,
    AccessVRAM = 3,
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

        VideoMode currentMode = VideoMode::AccessOAM;
        uint32_t cycleCounter = 0;
        uint32_t screenEnableDelay = 0;
        uint8_t vramBank = 0;
        uint8_t objectPriorityMode = 0;
        CGBPaletteRAM bgColorPalettes{0xFF}; // CGB startup background palettes are white (0x7FFF).
        CGBPaletteRAM objColorPalettes;
        std::array<uint8_t, GAMEBOY_WIDTH * GAMEBOY_HEIGHT> bgAttributePriority{};

        std::function<void(const FrameBuffer&)> vblankCallback;

        void setMode(VideoMode mode);
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

        bool windowTileMap() const;
        bool windowEnabled() const;
        bool bgWindowTileData() const;
        bool bgTileMapDisplay() const;
        bool spriteSize() const;
        bool spritesEnabled() const;
        bool bgEnabled() const;

        TileInfo getTileInfo(Address &tileSetLocation, uint8_t tileID, uint8_t tileLine) const;

        Color getRealColor(uint8_t pixelValue);
        ColorPalette loadPalette(ByteRegister &paletteRegister);
        Color getColorFromPalette(GBColor color, const ColorPalette &palette) const;
    public:
        PPU(Emulator &emulator);

        void tick(Cycles cycles);
        void setControlByte(uint8_t value);
        void registerVBlankCallback(const std::function<void(const FrameBuffer&)> &vblankCallback);

        uint8_t read(const Address &address);
        void write(const Address &address, uint8_t value);
        bool displayEnabled() const;
        bool cpuCanAccessVRAM() const;
        bool cpuCanAccessOAM() const;
        void setVRAMBank(uint8_t bank) { vramBank = bank & 1; } // Only bit 0 selects the bank
        uint8_t getVRAMBank() const { return vramBank; }
        void setObjectPriorityMode(uint8_t mode) { objectPriorityMode = mode & 1; } // Only bit 0 is used
        uint8_t getObjectPriorityMode() const { return objectPriorityMode; }
        uint8_t readCGBPaletteRegister(uint16_t address) const;
        void writeCGBPaletteRegister(uint16_t address, uint8_t value);
        uint8_t readVRAMBank(uint16_t offset, uint8_t bank) const;

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

        ByteRegister dmaTransfer;
};
