#pragma once

#include "FrameBuffer.hpp"
#include "MMU.hpp"
#include "Register.hpp"
#include "Common.hpp"
#include "Color.hpp"
#include <vector>
#include <functional>
#include <array>

namespace ppu
{
    // A scanline lasts 456 dots: mode 2 (OAM scan) takes 80, mode 3 (drawing)
    // at least 172, and mode 0 (HBlank) the rest. Mode 3 runs longer with
    // scrolling, the window and objects, which shortens mode 0 to match.
    // https://gbdev.io/pandocs/Rendering.html#ppu-modes
    static constexpr uint32_t CLOCKS_PER_HBLANK        = 204;  // Mode 0, with the shortest mode 3
    static constexpr uint32_t CLOCKS_PER_VBLANK        = 4560; // Mode 1
    static constexpr uint32_t CLOCKS_PER_SCANLINE_OAM  = 80;   // Mode 2
    static constexpr uint32_t CLOCKS_PER_SCANLINE_VRAM = 172;  // Mode 3, at its shortest

    // Mode 3 is drawn by a pixel fetcher feeding two pixel FIFOs, one for the
    // background/window and one for objects. Each FIFO holds up to 16 pixels.
    // The fetcher's Get Tile, Get Tile Data Low and Get Tile Data High steps
    // take 2 dots each; its Push step waits until the background FIFO is empty.
    // https://gbdev.io/pandocs/pixel_fifo.html#fifo-pixel-fetcher
    static constexpr uint8_t FIFO_CAPACITY   = 16;
    static constexpr uint8_t OBJ_FIFO_PIXELS = 8;
    static constexpr uint8_t FETCH_STEP_DOTS = 2;
    static constexpr uint8_t TILE_FETCH_DOTS = 3 * FETCH_STEP_DOTS; // Get Tile, Data Low and Data High

    // Tile data with LCDC.4 clear: signed tile numbers based at $9000
    // https://gbdev.io/pandocs/Tile_Data.html
    static constexpr uint16_t SIGNED_TILE_DATA_BASE = 0x1000;

    // An object fetch holds back the pixel output for the penalty Pan Docs
    // gives: 6 dots, plus the wait for the background fetch of the tile the
    // object starts in (once per tile).
    // https://gbdev.io/pandocs/Rendering.html#mode-3-length
    static constexpr uint32_t OBJ_FETCH_PENALTY       = 6;  // Fetching each object's tile
    static constexpr int32_t OBJ_X0_TILE              = -1;    // The off-screen tile of objects at OAM X = 0
    static constexpr int32_t OBJ_WINDOW_TILE_BASE     = 0x100; // Keeps window tiles apart from background ones
    static constexpr uint32_t OBJ_BG_FETCH_ALLOWANCE  = 2;  // Subtracted from the pixels left in the tile

    static constexpr uint32_t CLOCKS_PER_SCANLINE = (CLOCKS_PER_SCANLINE_OAM + CLOCKS_PER_SCANLINE_VRAM + CLOCKS_PER_HBLANK);
    static constexpr uint32_t SCANLINES_PER_FRAME = 144;
    static constexpr uint32_t CLOCKS_PER_FRAME    = (CLOCKS_PER_SCANLINE * SCANLINES_PER_FRAME) + CLOCKS_PER_VBLANK;
    
    // LY counts lines 0-153. Lines 144-153 are the
    // 10 invisible lines that make up VBlank.
    static constexpr uint32_t SCANLINES_PER_FRAME_WITH_VBLANK = 154;
    static constexpr uint32_t VBLANK_LINES = SCANLINES_PER_FRAME_WITH_VBLANK - SCANLINES_PER_FRAME;
    
    // Line 0 right after the LCD is switched on has no OAM scan (STAT reads
    // mode 0) and is 4 clocks shorter than a normal line, so LY becomes 1
    // after 452 clocks, as oam_bug/1-lcd_sync expects. Mode 3 starts this many
    // clocks after the switch: the PPU runs 2 clocks late on this line, as
    // mooneye's lcdon_timing-GS hardware tables show.
    static constexpr uint32_t LCD_ENABLE_DELAY = 78;
    static constexpr uint32_t LCD_ENABLE_FIRST_LINE_SHORTFALL = 4;
    
    // LCDC (LCD control register) bits
    static constexpr uint8_t LCDC_DISPLAY_ENABLE_BIT      = 7;
    static constexpr uint8_t LCDC_WINDOW_TILE_MAP_BIT     = 6;
    static constexpr uint8_t LCDC_WINDOW_ENABLE_BIT       = 5;
    static constexpr uint8_t LCDC_BG_WINDOW_TILE_DATA_BIT = 4;
    static constexpr uint8_t LCDC_BG_TILE_MAP_BIT         = 3;
    static constexpr uint8_t LCDC_SPRITE_SIZE_BIT         = 2;
    static constexpr uint8_t LCDC_SPRITES_ENABLE_BIT      = 1;
    static constexpr uint8_t LCDC_BG_ENABLE_BIT           = 0; // BG/window priority on the CGB
    
    // STAT (LCD status register) bits. Bits 0-1 hold the current
    // mode and bit 2 the LY == LYC flag; both are read-only.
    // Bits 3-6 enable the STAT interrupt sources and bit 7 is unused.
    static constexpr uint8_t STAT_MODE_MASK              = 0x03;
    static constexpr uint8_t STAT_COINCIDENCE_BIT        = 2;
    static constexpr uint8_t STAT_HBLANK_INTERRUPT_BIT   = 3;
    static constexpr uint8_t STAT_VBLANK_INTERRUPT_BIT   = 4;
    static constexpr uint8_t STAT_OAM_INTERRUPT_BIT      = 5;
    static constexpr uint8_t STAT_LYC_INTERRUPT_BIT      = 6;
    static constexpr uint8_t STAT_READ_ONLY_MASK         = 0x07;
    static constexpr uint8_t STAT_INTERRUPT_ENABLE_MASK  = 0x78;
    static constexpr uint8_t STAT_UNUSED_BITS            = 0x80;

    // IRQ48 signal bit positions (internal, not in register)
    static constexpr uint8_t IRQ48_HBLANK_BIT  = 0;
    static constexpr uint8_t IRQ48_VBLANK_BIT  = 1;
    static constexpr uint8_t IRQ48_OAM_BIT     = 2;
    static constexpr uint8_t IRQ48_LYC_BIT     = 3;

    // The mode bits in STAT, and the CPU's VRAM and OAM locks that follow
    // them, change at the first M-cycle boundary after the PPU itself changes
    // mode: this many dots later when the change falls on a boundary. The
    // STAT interrupt sources switch with the PPU, without the delay. Pan Docs
    // doesn't give this detail; mooneye's intr_2_mode0_timing,
    // intr_2_mode3_timing, intr_2_oam_ok_timing and
    // intr_2_mode0_timing_sprites pin it down.
    static constexpr uint32_t STAT_MODE_DELAY = 4;

    // The CPU's writes to the registers the PPU draws with (LCDC, SCY, SCX,
    // the palettes and WX) reach it this many dots into the M-cycle, rather
    // than at its end like other writes. Pan Docs doesn't give this; it
    // lines up Mealybug Tearoom's DMG hardware captures, and matches
    // SameBoy's DMG access-conflict timings.
    static constexpr uint32_t RENDER_REGISTER_WRITE_DOT = 0;

    // A pixel popped off the FIFOs reaches the LCD, where the palettes and
    // LCDC bits 0 and 1 are applied, this many dots later. On the DMG a
    // palette write first reads as the old value ORed with the new one for
    // one dot. Neither is in Pan Docs; both line up Mealybug Tearoom's DMG
    // hardware captures (SameBoy models the same effects).
    static constexpr uint8_t LCD_OUTPUT_LATENCY = 3;
    static constexpr uint8_t LCD_PIPELINE_CAPACITY = 8;
    static constexpr uint8_t PALETTE_GLITCH_DOTS = 1;
    static constexpr uint8_t LCD_CONTROL_OUTPUT_DELAY = 1; // LCDC bits 0 and 1 on the DMG

    // The VBlank interrupt and mode 1 STAT source at line 144, and the mode 2
    // STAT source at line 0, rise this many dots after their line starts; on
    // lines 1-143 the mode 2 source rises with the line. Pan Docs doesn't give
    // this. Mealybug Tearoom's tests compensate for it on line 0, which lines
    // up their DMG captures; doing the same at VBlank keeps mooneye's
    // intr_1_2_timing-GS interval, and SameBoy times both lines this way.
    static constexpr uint32_t LATE_VBLANK_DOTS = 4;

    // When LY changes, the LY=LYC flag reads 0 for one M-cycle, then compares
    // the new LY. Pan Docs only says the flag is constantly updated; the
    // hardware tables in mooneye's lcdon_timing-GS show the step.
    static constexpr uint32_t LY_COMPARE_DELAY = 4;

    // Sprites per scanline limit
    static constexpr uint32_t MAX_SPRITES_PER_LINE = 10;

    // LY=153 resets to 0 after this many T-cycles into that line
    static constexpr uint32_t LY153_EARLY_RESET_CLOCKS = 4;
    
    // Sprites (objects). OAM holds 40 entries of 4 bytes:
    // Y position, X position, tile number, and attributes.
    static constexpr uint32_t MAX_SPRITES               = 40;
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
    // On the DMG, WX = 166 makes the window span the whole screen, a line
    // down: the window carries on into the next line from its left edge
    // https://gbdev.io/pandocs/Window.html#window-rendering-criteria
    static constexpr uint8_t DMG_WX_BUG_VALUE = 166;
    
    // Attribute bits for sprites (OAM byte 3) and, on the Game
    // Boy Color, for background tiles (VRAM bank 1 tile map)
    static constexpr uint8_t ATTRIBUTE_CGB_PALETTE_MASK = 0x07;
    static constexpr uint8_t ATTRIBUTE_VRAM_BANK_BIT    = 3;
    static constexpr uint8_t ATTRIBUTE_DMG_PALETTE_BIT  = 4;
    static constexpr uint8_t ATTRIBUTE_X_FLIP_BIT       = 5;
    static constexpr uint8_t ATTRIBUTE_Y_FLIP_BIT       = 6;
    static constexpr uint8_t ATTRIBUTE_PRIORITY_BIT     = 7;
}

class Emulator;

// The values match the mode number reported in STAT bits 0-1
enum class VideoMode : uint8_t
{
    HBlank = 0,
    VBlank = 1,
    AccessOAM = 2,
    AccessVRAM = 3,
};

// A pixel in one of the FIFOs. Pan Docs: its color (0-3), its palette (CGB
// palette number, or the DMG object palette), its background priority (the
// OBJ-to-BG bit, or on the CGB the BG tile's priority bit) and, for objects
// on the CGB, the OAM index that decides which object is on top.
// https://gbdev.io/pandocs/pixel_fifo.html#introduction
class FifoPixel
{
    public:
        uint8_t color = 0;
        uint8_t palette = 0;
        bool bgPriority = false;
        uint8_t oamIndex = 0xFF;
};

// The fetcher's steps, in order
// https://gbdev.io/pandocs/pixel_fifo.html#fifo-pixel-fetcher
enum class FetchStep : uint8_t
{
    Tile,
    DataLow,
    DataHigh,
    Push,
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

        // The current line's length and the dot mode 3 started at (456 and 80
        // except on the first line after the LCD is switched on), and the mode
        // 0 length, known once mode 3 has drawn its 160 pixels
        uint32_t lineLength = ppu::CLOCKS_PER_SCANLINE;
        uint32_t mode3Start = ppu::CLOCKS_PER_SCANLINE_OAM;
        uint32_t hblankLength = ppu::CLOCKS_PER_HBLANK;
        uint32_t statModeDelay = 0; // Dots until STAT shows currentMode
        uint32_t lyCompareDelay = 0; // Dots until the LY=LYC flag compares the new LY
        uint32_t screenEnableDelay = 0;
        uint32_t dotsAhead = 0; // Dots of the coming M-cycle already run by tickAhead()
        uint8_t vramBank = 0;
        uint8_t objectPriorityMode = 0;
        CGBPaletteRAM bgColorPalettes{0xFF}; // CGB startup background palettes are white (0x7FFF).
        CGBPaletteRAM objColorPalettes;

        // Mode 3 state: the two pixel FIFOs, the fetcher, and the objects the
        // OAM scan found for this line
        // https://gbdev.io/pandocs/pixel_fifo.html
        std::array<FifoPixel, ppu::FIFO_CAPACITY> bgFifo{};
        uint8_t bgFifoHead = 0;
        uint8_t bgFifoSize = 0;
        std::array<FifoPixel, ppu::OBJ_FIFO_PIXELS> objFifo{};
        uint8_t objFifoSize = 0;

        FetchStep fetchStep = FetchStep::Tile;
        uint8_t fetchStepDots = 0;      // Dots spent in the current step
        bool firstFetch = false;        // The line's first fetch, which is thrown away
        bool fetcherStarting = false;   // The fetcher was just reset and waits a dot
        uint8_t fetcherIdleDots = 0;    // Dots the fetcher spends on a wasted fetch
        bool fetchingWindow = false;
        uint8_t fetcherX = 0;           // Tiles fetched since the line or the window started
        uint16_t fetchAddress = 0;      // The VRAM address the current step reads
        uint8_t fetchedTile = 0;
        uint8_t fetchedAttributes = 0;
        uint8_t fetchedLow = 0;
        uint8_t fetchedHigh = 0;

        uint32_t mode3Dots = 0;         // Dots mode 3 has run so far
        uint8_t pixelX = 0;             // The next pixel to send to the LCD
        uint8_t fineScrollShifted = 0;  // Pixels shifted out so far for SCX % 8
        bool fineScrollDone = false;
        uint8_t windowPixelsToShift = 0; // Window pixels left of the screen still to shift out
        uint8_t windowPixelsToDrop = 0; // Window pixels left of the screen when WX < 7
        int32_t windowStartX = 0;       // WX - 7 when the window started on this line
        uint8_t windowStartWX = 0;      // WX when the window started on this line
        bool insertBlankPixel = false;  // The WX-change bug's pixel goes out next
        bool windowFromLineStart = false; // DMG WX = 166: the next line starts in the window
        uint8_t nextLineWindowRow = 0;
        uint32_t objectStall = 0;       // Dots the output still waits for an object fetch
        uint8_t fetchingObject = 0;     // The OAM index of the object being fetched

        // Pixels on their way to the LCD
        class LCDPixel { public: FifoPixel bg; FifoPixel object; uint8_t x; uint8_t dotsLeft; };
        std::array<LCDPixel, ppu::LCD_PIPELINE_CAPACITY> lcdPipeline{};
        uint8_t lcdPipelineHead = 0;
        uint8_t lcdPipelineSize = 0;

        // A DMG palette write in its old-ORed-with-new dot
        ByteRegister *glitchedPalette = nullptr;
        uint8_t glitchedPaletteValue = 0;
        uint8_t paletteGlitchDots = 0;

        // LCDC as the LCD sees it: on the DMG, writes reach it a dot late
        uint8_t outputControl = 0;
        uint8_t pendingOutputControl = 0;
        uint8_t outputControlDots = 0;

        std::array<uint8_t, ppu::MAX_SPRITES_PER_LINE> lineObjects{}; // OAM indexes, by X then index
        uint8_t lineObjectCount = 0;
        uint8_t nextLineObject = 0;
        std::array<int32_t, ppu::MAX_SPRITES_PER_LINE> tilesWaitedFor{};
        uint8_t tilesWaitedForCount = 0;

        // The window's internal line counter: the window row drawn next. It
        // only counts lines the window was drawn on.
        // https://gbdev.io/pandocs/Window.html#window-rendering-criteria
        int32_t windowLine = 0;
        uint8_t windowRow = 0; // The row used by the window on this line
        bool windowYTriggered = false;

        // IRQ48 rising-edge gate: the STAT interrupt fires only when the
        // combined signal goes from 0 to non-zero (level→edge conversion).
        uint8_t irq48Signal = 0;

        // The DMG's brief mode 2 STAT source at the start of line 144
        bool line144OAMPulse = false;

        // Dots until VBlank's interrupts, or line 0's mode 2 STAT source, rise
        uint32_t lateInterruptDelay = 0;


        // LY=153 briefly shows 0 after just 4 T-clocks.
        bool ly153EarlyZero = false;

        // VBlank line counter inside the VBlank period (0-9)
        int32_t vblankLine = 0;

        std::function<void(const FrameBuffer&)> vblankCallback;

        void setMode(VideoMode mode);
        void showModeInSTAT();
        void refreshStatSignal(bool requestInterrupt);
        void raiseLateInterrupts();
        void compareLYToLYC();
        void startLYCompare();
        void checkWindowYTrigger();
        void scanOAM();
        void startMode3(uint32_t lineLength, uint32_t mode3Start);
        void runMode3();
        bool stepMode3();
        void stepFetcher();
        uint16_t tileDataAddress(uint8_t tile, uint8_t attributes, bool highByte) const;
        bool pushTileRow();
        uint8_t pixelsStillToDrop() const;
        bool startWindowIfReached();
        bool fetchObjectIfReached();
        uint32_t objectFetchDots(uint8_t oamX);
        uint8_t readObjectRow(uint8_t oamIndex, bool highByte);
        void mixObjectIntoFifo(uint8_t oamIndex, uint8_t low, uint8_t high);
        void popPixel();
        void advanceLCD(uint32_t dots);
        void drawPixel(const LCDPixel &pixel);
        void finishPaletteWrite();
        void draw();

        bool usesWindowTileMap() const;
        bool windowEnabled() const;
        bool bgWindowTileData() const;
        bool usesBackgroundTileMap() const;
        bool usesLargeSprites() const;
        bool spritesEnabled() const;
        bool bgEnabled() const;

        Color getRealColor(uint8_t pixelValue);
        ColorPalette loadPalette(ByteRegister &paletteRegister);
        Color getColorFromPalette(GBColor color, const ColorPalette &palette) const;
    public:
        PPU(Emulator &emulator);

        void tick(Cycles cycles);
        // Runs the first dots of the coming M-cycle now, for writes that land
        // partway through it. The next tick() skips them.
        void tickAhead(uint32_t dots);
        // How many dots into its M-cycle a CPU write to this register lands
        uint32_t registerWriteDot(uint16_t address, uint8_t value) const;
        void writeDMGPalette(ByteRegister &palette, uint8_t value);
        void setControlByte(uint8_t value);
        void writeSTAT(uint8_t value);
        void writeLYC(uint8_t value);
        void registerVBlankCallback(const std::function<void(const FrameBuffer&)> &vblankCallback);

        uint8_t read(const Address &address);
        void write(const Address &address, uint8_t value);
        bool displayEnabled() const;
        bool cpuCanAccessVRAM() const;
        bool cpuCanAccessOAM() const;
        bool cpuCanWriteVRAM() const;
        bool cpuCanWriteOAM() const;

        // The OAM row the PPU is scanning right now, or -1 when the OAM
        // corruption bug cannot happen (CGB, LCD off, not in mode 2)
        int oamScanRow() const;
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
