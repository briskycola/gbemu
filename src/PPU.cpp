#include "PPU.hpp"
#include "Color.hpp"
#include "Emulator.hpp"
#include "Common.hpp"
#include "Tile.hpp"
#include "IORegisters.hpp"
#include <stdexcept>
#include <algorithm>


PPU::PPU(Emulator &emulator) :
    emulator(emulator),
    buffer(common::GAMEBOY_WIDTH, common::GAMEBOY_HEIGHT),
    backgroundMap(common::BACKGROUND_MAP_SIZE, common::BACKGROUND_MAP_SIZE)
{
    videoRAM = std::vector<uint8_t>(mmu::VRAM_SIZE_BYTES);
    controlByte = 0;
}

uint8_t PPU::read(const Address &address)
{
    return videoRAM.at(static_cast<size_t>(vramBank) * mmu::VRAM_BANK_SIZE + address.getValue());
}

uint8_t PPU::readVRAMBank(uint16_t offset, uint8_t bank) const
{
    return videoRAM.at((bank & 1) * mmu::VRAM_BANK_SIZE + (offset & (mmu::VRAM_BANK_SIZE - 1)));
}

uint8_t PPU::readCGBPaletteRegister(uint16_t address) const
{
    switch (address)
    {
        case io::BG_PALETTE_INDEX_REGISTER:  return bgColorPalettes.readIndex();
        case io::BG_PALETTE_DATA_REGISTER:   return bgColorPalettes.readData();
        case io::OBJ_PALETTE_INDEX_REGISTER: return objColorPalettes.readIndex();
        default:     return objColorPalettes.readData();
    }
}

void PPU::writeCGBPaletteRegister(uint16_t address, uint8_t value)
{
    switch (address)
    {
        case io::BG_PALETTE_INDEX_REGISTER:  bgColorPalettes.writeIndex(value); break;
        case io::BG_PALETTE_DATA_REGISTER:   bgColorPalettes.writeData(value); break;
        case io::OBJ_PALETTE_INDEX_REGISTER: objColorPalettes.writeIndex(value); break;
        default:     objColorPalettes.writeData(value); break;
    }
}

void PPU::write(const Address &address, uint8_t value)
{
    videoRAM.at(static_cast<size_t>(vramBank) * mmu::VRAM_BANK_SIZE + address.getValue()) = value;
}

// The PPU switches mode now. STAT shows it at the next M-cycle boundary: at
// the end of this M-cycle if the switch fell partway through it (dots left
// over in cycleCounter), otherwise one M-cycle later.
void PPU::setMode(VideoMode mode)
{
    currentMode = mode;
    statModeDelay = ppu::STAT_MODE_DELAY;
    if (cycleCounter > 0) showModeInSTAT();
}

// https://gbdev.io/pandocs/STAT.html#ff41--stat-lcd-status
void PPU::showModeInSTAT()
{
    statModeDelay = 0;
    const uint8_t otherBits = lcdStatus.getValue() & static_cast<uint8_t>(~ppu::STAT_MODE_MASK);
    lcdStatus.set(otherBits | static_cast<uint8_t>(currentMode));
}

// Recompute the IRQ48 combined signal and fire a STAT interrupt on the
// rising edge (0 → non-zero). This matches the hardware "AND-OR" behavior:
// each interrupt source (HBlank, VBlank, OAM, LYC) is AND-ed with its enable
// bit, and the results are OR-ed. The CPU receives a pulse only when the
// combined result transitions from 0 to non-zero.
// https://gbdev.io/pandocs/Interrupt_Sources.html#int-48--stat-interrupt
void PPU::refreshStatSignal(bool requestInterrupt)
{
    // With the PPU off nothing drives the sources, so the line keeps its level
    if (!displayEnabled()) return;

    uint8_t signal = 0;
    const uint8_t stat = lcdStatus.getValue();
    if (checkBit(stat, ppu::STAT_HBLANK_INTERRUPT_BIT) && currentMode == VideoMode::HBlank)
        signal = setBit(signal, ppu::IRQ48_HBLANK_BIT);
    if (checkBit(stat, ppu::STAT_VBLANK_INTERRUPT_BIT) && currentMode == VideoMode::VBlank)
        signal = setBit(signal, ppu::IRQ48_VBLANK_BIT);
    if (checkBit(stat, ppu::STAT_OAM_INTERRUPT_BIT) && (currentMode == VideoMode::AccessOAM || line144OAMPulse))
        signal = setBit(signal, ppu::IRQ48_OAM_BIT);
    if (checkBit(stat, ppu::STAT_LYC_INTERRUPT_BIT) && checkBit(stat, ppu::STAT_COINCIDENCE_BIT))
        signal = setBit(signal, ppu::IRQ48_LYC_BIT);

    if (requestInterrupt && (irq48Signal == 0) && (signal != 0))
    {
        emulator.cpu.interruptFlag.setBitTo(cpu::INTERRUPT_LCD_STATUS_BIT, true);
    }

    irq48Signal = signal;
}

// Update LY==LYC flag and fire STAT if needed.
// Writing STAT sets its interrupt selects, which can raise the STAT line.
// On the DMG the write first acts as if $FF were written, so it can request
// a spurious interrupt during OAM scan, HBlank, VBlank or LY=LYC.
// https://gbdev.io/pandocs/STAT.html#spurious-stat-interrupts
void PPU::writeSTAT(uint8_t value)
{
    const uint8_t readOnly = lcdStatus.getValue() & ppu::STAT_READ_ONLY_MASK;
    if (!emulator.cgbMode)
    {
        lcdStatus.set(readOnly | ppu::STAT_INTERRUPT_ENABLE_MASK | ppu::STAT_UNUSED_BITS);
        refreshStatSignal(true);
    }
    lcdStatus.set(readOnly | (value & ppu::STAT_INTERRUPT_ENABLE_MASK) | ppu::STAT_UNUSED_BITS);
    refreshStatSignal(true);
}

// LY=LYC is constantly compared while the PPU runs, so a new LYC takes
// effect at once
// https://gbdev.io/pandocs/STAT.html#ff45--lyc-ly-compare
void PPU::writeLYC(uint8_t value)
{
    lyCompare.set(value);
    compareLYToLYC();
}

// When LY changes the LY=LYC flag reads 0 at once, and compares the new LY
// one M-cycle later
void PPU::startLYCompare()
{
    lcdStatus.setBitTo(ppu::STAT_COINCIDENCE_BIT, false);
    refreshStatSignal(true);
    lyCompareDelay = ppu::LY_COMPARE_DELAY;
}

void PPU::compareLYToLYC()
{
    if (!displayEnabled()) return;
    const bool coincidence = (lyCompare.getValue() == line.getValue());
    lcdStatus.setBitTo(ppu::STAT_COINCIDENCE_BIT, coincidence);
    refreshStatSignal(true);
}

// Check whether the window Y trigger should be set for the current LY.
// Must be called at the start of every OAM scan (mode 2 entry).
void PPU::checkWindowYTrigger()
{
    if (!windowYTriggered && windowEnabled() &&
        line.getValue() == windowY.getValue())
    {
        windowYTriggered = true;
    }
}

// Whether the window appears on the current line: it is enabled, WY has
// matched LY this frame, and WX puts it on screen
bool PPU::windowShownOnLine() const
{
    return windowEnabled() && windowYTriggered &&
        windowX.getValue() < common::GAMEBOY_WIDTH + ppu::WINDOW_X_OFFSET;
}

// Mode 3 lasts 172 dots plus the penalties Pan Docs lists: SCX % 8 for the
// discarded pixels, 6 if the window starts, and 6 to 11 for each object drawn
// https://gbdev.io/pandocs/Rendering.html#mode-3-length
uint32_t PPU::calculateMode3Length() const
{
    const uint8_t scx = scrollX.getValue();
    uint32_t length = ppu::CLOCKS_PER_SCANLINE_VRAM + scx % ppu::TILE_WIDTH_PX;

    const bool window = windowShownOnLine();
    const int32_t windowStart = static_cast<int32_t>(windowX.getValue()) - static_cast<int32_t>(ppu::WINDOW_X_OFFSET);
    if (window) length += ppu::WINDOW_PENALTY;

    if (!spritesEnabled()) return length;

    // The objects the OAM scan found for this line: the first 10 in OAM order
    const int32_t spriteHeight = usesLargeSprites() ? 16 : 8;
    const int32_t currentLine = line.getValue();
    std::vector<std::pair<uint8_t, uint8_t>> objects; // OAM X, OAM index
    for (uint8_t i = 0; i < ppu::MAX_SPRITES && objects.size() < ppu::MAX_SPRITES_PER_LINE; ++i)
    {
        const uint16_t oamBase = mmu::OAM_ADDRESS_START + i * ppu::SPRITE_BYTES;
        const int32_t spriteY = static_cast<int32_t>(emulator.mmu.read(Address(oamBase + ppu::OAM_SPRITE_Y_BYTE), true)) - ppu::SPRITE_Y_OFFSET;
        if (currentLine >= spriteY && currentLine < spriteY + spriteHeight)
            objects.emplace_back(emulator.mmu.read(Address(oamBase + ppu::OAM_SPRITE_X_BYTE), true), i);
    }

    // OBJ penalty algorithm: objects are considered from left to right,
    // ties broken by OAM index
    // https://gbdev.io/pandocs/Rendering.html#obj-penalty-algorithm
    std::sort(objects.begin(), objects.end());
    std::vector<int32_t> tilesConsidered;
    for (const auto &object : objects)
    {
        const uint8_t oamX = object.first;
        if (oamX >= common::GAMEBOY_WIDTH + ppu::SPRITE_X_OFFSET) continue; // Entirely off the right edge

        // The background or window tile that the object's leftmost pixel is
        // in, and how many of that tile's pixels are to its right. An object
        // at OAM X = 0 is off the left edge, in a tile of its own whatever
        // SCX is: the first one costs the 11 dots Pan Docs gives, and others
        // at X = 0 share that tile, as mooneye's intr_2_mode0_timing_sprites
        // shows.
        int32_t tile = ppu::OBJ_X0_TILE;
        int32_t pixelsToTheRight = static_cast<int32_t>(ppu::TILE_WIDTH_PX) - 1;
        if (oamX != 0)
        {
            const int32_t pixel = static_cast<int32_t>(oamX) - ppu::SPRITE_X_OFFSET;
            const bool inWindow = window && pixel >= windowStart;
            const int32_t coordinate = inWindow ? pixel - windowStart : (pixel + scx) & (common::BACKGROUND_MAP_SIZE - 1);
            tile = (inWindow ? ppu::OBJ_WINDOW_TILE_BASE : 0) + coordinate / static_cast<int32_t>(ppu::TILE_WIDTH_PX);
            pixelsToTheRight = static_cast<int32_t>(ppu::TILE_WIDTH_PX) - 1 - coordinate % static_cast<int32_t>(ppu::TILE_WIDTH_PX);
        }

        // Waiting for that tile's fetch to finish, once per tile
        if (std::find(tilesConsidered.begin(), tilesConsidered.end(), tile) == tilesConsidered.end())
        {
            tilesConsidered.push_back(tile);
            length += static_cast<uint32_t>(std::max(0, pixelsToTheRight - static_cast<int32_t>(ppu::OBJ_BG_FETCH_ALLOWANCE)));
        }
        length += ppu::OBJ_FETCH_PENALTY;
    }
    return length;
}

// Starts drawing a line: mode 3 gets its length, and mode 0 the rest of the
// line. lineLength and mode3Start are 456 and 80 except on the first line
// after the LCD is switched on.
void PPU::startMode3(uint32_t lineLength, uint32_t mode3Start)
{
    mode3Length = calculateMode3Length();
    hblankLength = lineLength - mode3Start - mode3Length;
    setMode(VideoMode::AccessVRAM);
}

void PPU::tick(Cycles cycles)
{
    if (!displayEnabled()) return;

    // The LY=LYC flag compares a new LY one M-cycle after LY changes
    if (lyCompareDelay > 0)
    {
        lyCompareDelay = cycles.cycles >= lyCompareDelay ? 0 : lyCompareDelay - cycles.cycles;
        if (lyCompareDelay == 0) compareLYToLYC();
    }

    // STAT catches up with a mode change after STAT_MODE_DELAY dots
    if (statModeDelay > 0)
    {
        statModeDelay = cycles.cycles >= statModeDelay ? 0 : statModeDelay - cycles.cycles;
        if (statModeDelay == 0) showModeInSTAT();
    }

    uint32_t elapsed = cycles.cycles;

    if (screenEnableDelay > 0)
    {
        if (elapsed < screenEnableDelay)
        {
            screenEnableDelay -= elapsed;
            return;
        }
        // Only the clocks past the end of the delay count toward mode 3. They
        // go into cycleCounter now, so mode 3 knows it started partway through
        // this M-cycle.
        cycleCounter = elapsed - screenEnableDelay;
        elapsed = 0;
        screenEnableDelay = 0;
        line.set(0);
        windowLine = 0;
        windowYTriggered = false;
        vblankLine = 0;
        ly153EarlyZero = false;
        colorCache.fill(0);
        // No OAM scan on this first line: go straight to drawing. The line is
        // 4 dots short, and drawing starts LCD_ENABLE_DELAY dots in.
        checkWindowYTrigger();
        startMode3(ppu::CLOCKS_PER_SCANLINE - ppu::LCD_ENABLE_FIRST_LINE_SHORTFALL, ppu::LCD_ENABLE_DELAY);
        compareLYToLYC();
        refreshStatSignal(true);
    }

    cycleCounter += elapsed;

    switch (currentMode)
    {
        case VideoMode::AccessOAM:
        {
            if (cycleCounter >= ppu::CLOCKS_PER_SCANLINE_OAM)
            {
                cycleCounter -= ppu::CLOCKS_PER_SCANLINE_OAM;
                startMode3(ppu::CLOCKS_PER_SCANLINE, ppu::CLOCKS_PER_SCANLINE_OAM);
                refreshStatSignal(true);
            }
            break;
        }

        case VideoMode::AccessVRAM:
        {
            if (cycleCounter >= mode3Length)
            {
                cycleCounter -= mode3Length;
                setMode(VideoMode::HBlank);

                // Render this scanline at the transition to HBlank.
                // Clear the current line's color-cache strip first so stale
                // pixels from earlier in the frame can't affect sprite priority.
                const uint32_t lineBase = static_cast<uint32_t>(line.getValue()) * common::GAMEBOY_WIDTH;
                for (uint32_t i = 0; i < common::GAMEBOY_WIDTH; ++i)
                    colorCache[lineBase + i] = 0;

                writeScanline(line.getValue());

                // Copy one HBlank DMA block. Only the CPU stalls while it
                // runs (through dmaCyclesRemaining); the PPU keeps its own
                // timing, so HBlank is neither lengthened nor shortened.
                emulator.mmu.hblankDMA();

                refreshStatSignal(true);
            }
            break;
        }

        case VideoMode::HBlank:
        {
            if (cycleCounter >= hblankLength)
            {
                cycleCounter -= hblankLength;

                line.increment();

                if (line == ppu::SCANLINES_PER_FRAME)
                {
                    // Entering VBlank
                    setMode(VideoMode::VBlank);
                    vblankLine = 0;

                    // Fire VBlank interrupt
                    emulator.cpu.interruptFlag.setBitTo(cpu::INTERRUPT_VBLANK_BIT, true);

                    // Render the finished frame before resetting anything
                    draw();
                    buffer.reset();
                    colorCache.fill(0);

                    // Window state resets for the next frame
                    windowLine = 0;
                    windowYTriggered = false;

                    // On the DMG, line 144 starts with a mode 2 pulse, so the
                    // mode 2 STAT interrupt also fires when VBlank begins.
                    // (mooneye's vblank_stat_intr-GS; the CGB doesn't do this.)
                    line144OAMPulse = !emulator.cgbMode;
                    startLYCompare();
                    refreshStatSignal(true);
                    line144OAMPulse = false;
                    refreshStatSignal(true);
                }
                else
                {
                    setMode(VideoMode::AccessOAM);
                    startLYCompare();
                    // Check WY trigger for the newly started line
                    checkWindowYTrigger();
                    refreshStatSignal(true);
                }
            }
            break;
        }

        case VideoMode::VBlank:
        {
            // LY=153 special case: LY briefly shows 0 after just 4 T-cycles
            if (line.getValue() == ppu::SCANLINES_PER_FRAME_WITH_VBLANK - 1 && !ly153EarlyZero)
            {
                if (cycleCounter >= ppu::LY153_EARLY_RESET_CLOCKS)
                {
                    ly153EarlyZero = true;
                    line.set(0);
                    compareLYToLYC();
                }
            }

            if (cycleCounter >= ppu::CLOCKS_PER_SCANLINE)
            {
                cycleCounter -= ppu::CLOCKS_PER_SCANLINE;
                vblankLine++;

                if (vblankLine >= 10)
                {
                    // End of VBlank: restart rendering at line 0
                    ly153EarlyZero = false;
                    vblankLine = 0;
                    line.set(0);
                    setMode(VideoMode::AccessOAM);
                    checkWindowYTrigger();
                    compareLYToLYC();
                    refreshStatSignal(true);
                }
                else
                {
                    if (!ly153EarlyZero)
                    {
                        line.increment();
                        startLYCompare();
                    }
                }
            }
            break;
        }
    }
}

void PPU::setControlByte(uint8_t value)
{
    const bool wasEnabled = displayEnabled();
    const bool nowEnabled = checkBit(value, ppu::LCDC_DISPLAY_ENABLE_BIT);
    controlByte = value;

    if (wasEnabled && !nowEnabled)
    {
        screenEnableDelay = 0;
        cycleCounter = 0;
        setMode(VideoMode::HBlank);
        line.set(0);
        windowLine = 0;
        windowYTriggered = false;
        vblankLine = 0;
        ly153EarlyZero = false;
        lyCompareDelay = 0;
        // STAT reads mode 0 while the PPU is off. The LY=LYC flag and the STAT
        // line keep their values: nothing updates them until it is back on.
        showModeInSTAT();
        buffer.reset();
        colorCache.fill(0);
    }
    else if (!wasEnabled && nowEnabled)
    {
        screenEnableDelay = ppu::LCD_ENABLE_DELAY;
        cycleCounter = 0;
        setMode(VideoMode::HBlank);
        showModeInSTAT();
        line.set(0);

        // The LY=LYC comparison runs again from now, with LY at 0
        compareLYToLYC();
    }
}

bool PPU::displayEnabled() const { return checkBit(controlByte, ppu::LCDC_DISPLAY_ENABLE_BIT); }
// The CPU can't reach VRAM during mode 3, or OAM during modes 2 and 3. A lock
// starts as soon as the PPU enters the mode, but ends only once STAT shows
// the next one (mooneye's lcdon_timing-GS hardware tables).
// https://gbdev.io/pandocs/Rendering.html#ppu-modes
bool PPU::cpuCanAccessVRAM() const
{
    if (!displayEnabled()) return true;
    const auto shownMode = static_cast<VideoMode>(lcdStatus.getValue() & ppu::STAT_MODE_MASK);
    return currentMode != VideoMode::AccessVRAM && shownMode != VideoMode::AccessVRAM;
}

bool PPU::cpuCanAccessOAM() const
{
    if (!displayEnabled()) return true;
    const auto blocks = [](VideoMode mode) { return mode == VideoMode::AccessOAM || mode == VideoMode::AccessVRAM; };
    return !blocks(currentMode) && !blocks(static_cast<VideoMode>(lcdStatus.getValue() & ppu::STAT_MODE_MASK));
}

// Writes follow the mode STAT shows. One exception: in the M-cycle where the
// PPU has gone from mode 2 to mode 3 but STAT still shows mode 2, OAM writes
// go through (mooneye's lcdon_write_timing-GS hardware tables).
bool PPU::cpuCanWriteVRAM() const
{
    if (!displayEnabled()) return true;
    return static_cast<VideoMode>(lcdStatus.getValue() & ppu::STAT_MODE_MASK) != VideoMode::AccessVRAM;
}

bool PPU::cpuCanWriteOAM() const
{
    if (!displayEnabled()) return true;
    const auto shownMode = static_cast<VideoMode>(lcdStatus.getValue() & ppu::STAT_MODE_MASK);
    if (shownMode == VideoMode::AccessVRAM) return false;
    return shownMode != VideoMode::AccessOAM || currentMode == VideoMode::AccessVRAM;
}

int PPU::oamScanRow() const
{
    // The bug only exists on the DMG, and line 0 right after the LCD is
    // switched on has no OAM scan
    if (emulator.cgbMode || !displayEnabled() || screenEnableDelay > 0) return -1;
    if (currentMode != VideoMode::AccessOAM) return -1;
    const int row = static_cast<int>(cycleCounter / mmu::OAM_SCAN_CLOCKS_PER_ROW);
    return row < mmu::OAM_ROW_COUNT ? row : -1;
}

bool PPU::usesWindowTileMap() const { return checkBit(controlByte, ppu::LCDC_WINDOW_TILE_MAP_BIT); }
bool PPU::windowEnabled() const { return checkBit(controlByte, ppu::LCDC_WINDOW_ENABLE_BIT); }
bool PPU::bgWindowTileData() const { return checkBit(controlByte, ppu::LCDC_BG_WINDOW_TILE_DATA_BIT); }
bool PPU::usesBackgroundTileMap() const { return checkBit(controlByte, ppu::LCDC_BG_TILE_MAP_BIT); }
bool PPU::usesLargeSprites() const { return checkBit(controlByte, ppu::LCDC_SPRITE_SIZE_BIT); }
bool PPU::spritesEnabled() const { return checkBit(controlByte, ppu::LCDC_SPRITES_ENABLE_BIT); }
bool PPU::bgEnabled() const { return checkBit(controlByte, ppu::LCDC_BG_ENABLE_BIT); }

void PPU::writeScanline(uint8_t currentLine)
{
    if (!displayEnabled()) return;

    if (bgEnabled() || emulator.cgbMode)
    {
        drawBGLine(currentLine);
    }

    if (windowEnabled() && windowYTriggered)
    {
        drawWindowLine(currentLine);
    }

    drawSpritesForLine(currentLine);
}

void PPU::drawBGLine(uint32_t currentLine)
{
    bool useTileSetZero = bgWindowTileData();
    bool useTileMapZero = !usesBackgroundTileMap();

    ColorPalette palette = loadPalette(bgPalette);

    Address tileSetAddress = useTileSetZero ? ppu::TILE_SET_ZERO_ADDRESS : ppu::TILE_SET_ONE_ADDRESS;
    Address tileMapAddress = useTileMapZero ? ppu::TILE_MAP_ZERO_ADDRESS : ppu::TILE_MAP_ONE_ADDRESS;

    uint32_t screenY = currentLine;

    for (uint32_t screenX = 0; screenX < common::GAMEBOY_WIDTH; screenX++)
    {
        uint32_t scrolledX = screenX + scrollX.getValue();
        uint32_t scrolledY = screenY + scrollY.getValue();

        uint32_t bgMapX = scrolledX % common::BACKGROUND_MAP_SIZE;
        uint32_t bgMapY = scrolledY % common::BACKGROUND_MAP_SIZE;

        uint32_t tileX = bgMapX / ppu::TILE_WIDTH_PX;
        uint32_t tileY = bgMapY / ppu::TILE_HEIGHT_PX;

        uint32_t tilePixelX = bgMapX % ppu::TILE_WIDTH_PX;
        uint32_t tilePixelY = bgMapY % ppu::TILE_HEIGHT_PX;

        uint32_t tileIndex = tileY * ppu::TILES_PER_LINE + tileX;
        Address tileIDAddress = tileMapAddress + tileIndex;

        // Tile numbers always live in VRAM bank 0 and attributes in bank 1,
        // whichever bank the CPU currently has selected through VBK
        uint8_t tileID = readVRAMBank(tileIDAddress.getValue() - mmu::VRAM_ADDRESS_START, 0);
        uint8_t tileAttributes = emulator.cgbMode
            ? readVRAMBank(tileIDAddress.getValue() - mmu::VRAM_ADDRESS_START, 1) : 0;

        uint32_t tileDataMemoryOffset = useTileSetZero
            ? tileID * ppu::TILE_BYTES
            : (static_cast<int8_t>(tileID) + 128) * ppu::TILE_BYTES;

        uint32_t row    = checkBit(tileAttributes, ppu::ATTRIBUTE_Y_FLIP_BIT) ? (ppu::TILE_HEIGHT_PX - 1) - tilePixelY : tilePixelY;
        uint32_t column = checkBit(tileAttributes, ppu::ATTRIBUTE_X_FLIP_BIT) ? (ppu::TILE_WIDTH_PX  - 1) - tilePixelX : tilePixelX;
        uint32_t tileDataOffset = tileSetAddress.getValue() - mmu::VRAM_ADDRESS_START + tileDataMemoryOffset + row * ppu::TILE_BYTES_PER_ROW;
        const uint8_t tileBank = getBitValue(tileAttributes, ppu::ATTRIBUTE_VRAM_BANK_BIT);
        uint8_t pixelsOne = readVRAMBank(tileDataOffset, tileBank);
        uint8_t pixelsTwo = readVRAMBank(tileDataOffset + 1, tileBank);
        GBColor pixelColor = getPixelFromLine(pixelsOne, pixelsTwo, column);
        const uint8_t shade = static_cast<uint8_t>(pixelColor);

        // Store color cache for sprite priority resolution
        // bits 1-0: shade, bit 2: BG priority flag (CGB only)
        uint8_t cache = shade & 0x03;
        if (emulator.cgbMode && checkBit(tileAttributes, ppu::ATTRIBUTE_PRIORITY_BIT) && bgEnabled())
            cache = setBit(cache, 2);
        colorCache[screenY * common::GAMEBOY_WIDTH + screenX] = cache;

        if (emulator.cgbMode)
            buffer.setPixelRGB(screenX, screenY, bgColorPalettes.getColor(tileAttributes & ppu::ATTRIBUTE_CGB_PALETTE_MASK, shade), shade);
        else if (emulator.sgbMode)
        {
            const uint8_t mappedShade = static_cast<uint8_t>(getColorFromPalette(pixelColor, palette));
            const uint8_t paletteIndex = emulator.sgb.getAttribute(screenX, screenY);
            buffer.setPixelRGB(screenX, screenY, emulator.sgb.getColor(paletteIndex, mappedShade, screenX, screenY), mappedShade);
        }
        else
            buffer.setPixel(screenX, screenY, getColorFromPalette(pixelColor, palette));
    }
}

void PPU::drawWindowLine(uint32_t currentLine)
{
    // WX<7 clips the window to screen left edge
    int32_t wx = static_cast<int32_t>(windowX.getValue()) - static_cast<int32_t>(ppu::WINDOW_X_OFFSET);
    if (wx > static_cast<int32_t>(common::GAMEBOY_WIDTH) - 1) return;

    bool useTileSetZero = bgWindowTileData();
    bool useTileMapZero = !usesWindowTileMap();

    ColorPalette palette = loadPalette(bgPalette);

    Address tileSetAddress = useTileSetZero ? ppu::TILE_SET_ZERO_ADDRESS : ppu::TILE_SET_ONE_ADDRESS;
    Address tileMapAddress = useTileMapZero ? ppu::TILE_MAP_ZERO_ADDRESS : ppu::TILE_MAP_ONE_ADDRESS;

    uint32_t screenY = currentLine;
    uint32_t lineAdjusted = static_cast<uint32_t>(windowLine);

    uint32_t tileY      = lineAdjusted / ppu::TILE_HEIGHT_PX;
    uint32_t tilePixelY = lineAdjusted % ppu::TILE_HEIGHT_PX;

    // Iterate tile by tile across the screen
    int32_t lastTileX = (static_cast<int32_t>(common::GAMEBOY_WIDTH) - 1 - wx) / static_cast<int32_t>(ppu::TILE_WIDTH_PX);

    for (int32_t tileX = 0; tileX <= lastTileX; tileX++)
    {
        uint32_t tileIndex = tileY * ppu::TILES_PER_LINE + static_cast<uint32_t>(tileX);
        Address tileIDAddress = tileMapAddress + tileIndex;

        uint8_t tileID = readVRAMBank(tileIDAddress.getValue() - mmu::VRAM_ADDRESS_START, 0);
        uint8_t tileAttributes = emulator.cgbMode
            ? readVRAMBank(tileIDAddress.getValue() - mmu::VRAM_ADDRESS_START, 1) : 0;

        uint32_t tileDataMemoryOffset = useTileSetZero
            ? tileID * ppu::TILE_BYTES
            : (static_cast<int8_t>(tileID) + 128) * ppu::TILE_BYTES;

        uint32_t row =checkBit(tileAttributes, ppu::ATTRIBUTE_Y_FLIP_BIT)
            ? (ppu::TILE_HEIGHT_PX - 1) - tilePixelY : tilePixelY;
        uint32_t tileDataOffset = tileSetAddress.getValue() - mmu::VRAM_ADDRESS_START + tileDataMemoryOffset + row * ppu::TILE_BYTES_PER_ROW;
        const uint8_t tileBank = getBitValue(tileAttributes, ppu::ATTRIBUTE_VRAM_BANK_BIT);
        uint8_t pixelsOne = readVRAMBank(tileDataOffset, tileBank);
        uint8_t pixelsTwo = readVRAMBank(tileDataOffset + 1, tileBank);

        for (uint32_t pixelX = 0; pixelX < ppu::TILE_WIDTH_PX; pixelX++)
        {
            int32_t screenX = wx + tileX * static_cast<int32_t>(ppu::TILE_WIDTH_PX) + static_cast<int32_t>(pixelX);
            if (screenX < 0 || screenX >= static_cast<int32_t>(common::GAMEBOY_WIDTH)) continue;

            uint32_t column = checkBit(tileAttributes, ppu::ATTRIBUTE_X_FLIP_BIT)
                ? (ppu::TILE_WIDTH_PX - 1) - pixelX : pixelX;

            GBColor pixelColor = getPixelFromLine(pixelsOne, pixelsTwo, column);
            const uint8_t shade = static_cast<uint8_t>(pixelColor);

            uint8_t cache = shade & 0x03;
            if (emulator.cgbMode && checkBit(tileAttributes, ppu::ATTRIBUTE_PRIORITY_BIT) && bgEnabled())
                cache = setBit(cache, 2);
            colorCache[screenY * common::GAMEBOY_WIDTH + static_cast<uint32_t>(screenX)] = cache;

            if (emulator.cgbMode)
                buffer.setPixelRGB(static_cast<uint32_t>(screenX), screenY, bgColorPalettes.getColor(tileAttributes & ppu::ATTRIBUTE_CGB_PALETTE_MASK, shade), shade);
            else if (emulator.sgbMode)
            {
                const uint8_t mappedShade = static_cast<uint8_t>(getColorFromPalette(pixelColor, palette));
                const uint8_t paletteIndex = emulator.sgb.getAttribute(static_cast<uint32_t>(screenX), screenY);
                buffer.setPixelRGB(static_cast<uint32_t>(screenX), screenY, emulator.sgb.getColor(paletteIndex, mappedShade, static_cast<uint32_t>(screenX), screenY), mappedShade);
            }
            else
                buffer.setPixel(static_cast<uint32_t>(screenX), screenY, getColorFromPalette(pixelColor, palette));
        }
    }

    windowLine++;
}

// Collect up to 10 sprites visible on this line, then render them back-to-front.
void PPU::drawSpritesForLine(uint32_t currentLine)
{
    if (!spritesEnabled()) return;

    const int32_t spriteHeight = usesLargeSprites() ? 16 : 8;
    const int32_t line = static_cast<int32_t>(currentLine);

    // Collect up to 10 sprites visible on this line (OAM order = priority)
    uint8_t visible[ppu::MAX_SPRITES_PER_LINE];
    uint32_t visibleCount = 0;

    for (uint8_t i = 0; i < ppu::MAX_SPRITES && visibleCount < ppu::MAX_SPRITES_PER_LINE; i++)
    {
        const uint16_t oamBase = mmu::OAM_ADDRESS_START + i * ppu::SPRITE_BYTES;
        const int32_t spriteY = static_cast<int32_t>(emulator.mmu.read(Address(oamBase + ppu::OAM_SPRITE_Y_BYTE), true)) - ppu::SPRITE_Y_OFFSET;
        if (line >= spriteY && line < spriteY + spriteHeight)
            visible[visibleCount++] = i;
    }

    // On DMG: sort by X then OAM index; on CGB with OAM priority: OAM index only.
    // Draw lowest-priority first so highest-priority overwrites them.
    const bool oamPriority = emulator.cgbMode && (objectPriorityMode == 0);
    if (!oamPriority)
    {
        // Sort so that lower X (and among equal X, lower OAM index) wins.
        // We draw in reverse order (last in array drawn first = lowest priority first).
        std::stable_sort(visible, visible + visibleCount, [this](uint8_t a, uint8_t b)
        {
            const uint8_t xA = emulator.mmu.read(Address(mmu::OAM_ADDRESS_START + a * ppu::SPRITE_BYTES + ppu::OAM_SPRITE_X_BYTE), true);
            const uint8_t xB = emulator.mmu.read(Address(mmu::OAM_ADDRESS_START + b * ppu::SPRITE_BYTES + ppu::OAM_SPRITE_X_BYTE), true);
            return xA > xB; // higher X = lower priority; draw first
        });
    }
    // For CGB OAM priority mode: higher OAM index = lower priority (draw first).
    // visible[] is already in OAM order (ascending), so reversing gives descending = lower priority first.
    else
    {
        std::reverse(visible, visible + visibleCount);
    }

    for (uint32_t i = 0; i < visibleCount; i++)
        drawSprite(line, visible[i], spriteHeight);
}

void PPU::drawSprite(int32_t line, uint8_t spriteIndex, int32_t spriteHeight)
{
    const uint16_t oamBase = mmu::OAM_ADDRESS_START + spriteIndex * ppu::SPRITE_BYTES;
    const int32_t spriteY = static_cast<int32_t>(emulator.mmu.read(Address(oamBase + ppu::OAM_SPRITE_Y_BYTE), true)) - ppu::SPRITE_Y_OFFSET;
    const int32_t spriteX = static_cast<int32_t>(emulator.mmu.read(Address(oamBase + ppu::OAM_SPRITE_X_BYTE), true)) - ppu::SPRITE_X_OFFSET;

    if (spriteX <= -8 || spriteX >= static_cast<int32_t>(common::GAMEBOY_WIDTH)) return;

    uint8_t pattern = emulator.mmu.read(Address(oamBase + ppu::OAM_SPRITE_TILE_BYTE), true);
    const uint8_t spriteAttributes = emulator.mmu.read(Address(oamBase + ppu::OAM_SPRITE_ATTRIBUTE_BYTE), true);

    // For 8x16 sprites bit 0 of the tile number is ignored
    if (spriteHeight == 16) pattern &= 0xFE;

    const bool flipX = checkBit(spriteAttributes, ppu::ATTRIBUTE_X_FLIP_BIT);
    const bool flipY = checkBit(spriteAttributes, ppu::ATTRIBUTE_Y_FLIP_BIT);
    const bool objectBehindBG = checkBit(spriteAttributes, ppu::ATTRIBUTE_PRIORITY_BIT);

    // Which row of the sprite is visible on this scanline?
    int32_t pixelY = line - spriteY;
    if (flipY) pixelY = (spriteHeight - 1) - pixelY;

    // For 8x16, the second tile follows immediately (tile number + 1)
    int32_t tileOffset = 0;
    int32_t rowInTile  = pixelY;
    if (spriteHeight == 16 && pixelY >= 8)
    {
        tileOffset = 16; // second tile bytes
        rowInTile  = pixelY - 8;
    }

    const uint8_t tileBank = emulator.cgbMode ? getBitValue(spriteAttributes, ppu::ATTRIBUTE_VRAM_BANK_BIT) : 0;
    const uint16_t tileAddr = static_cast<uint16_t>(
        (ppu::TILE_SET_ZERO_ADDRESS.getValue() - mmu::VRAM_ADDRESS_START) +
        static_cast<uint16_t>(pattern) * ppu::TILE_BYTES +
        static_cast<uint16_t>(tileOffset) +
        static_cast<uint16_t>(rowInTile) * ppu::TILE_BYTES_PER_ROW);

    const uint8_t byte1 = readVRAMBank(tileAddr,     tileBank);
    const uint8_t byte2 = readVRAMBank(tileAddr + 1, tileBank);

    // Load DMG palette
    const bool usePaletteOne = checkBit(spriteAttributes, ppu::ATTRIBUTE_DMG_PALETTE_BIT);
    const ColorPalette palette = usePaletteOne ? loadPalette(spritePalette1) : loadPalette(spritePalette0);
    const uint8_t cgbPal = spriteAttributes & ppu::ATTRIBUTE_CGB_PALETTE_MASK;

    for (int32_t px = 0; px < static_cast<int32_t>(ppu::TILE_WIDTH_PX); px++)
    {
        const int32_t screenX = spriteX + px;
        if (screenX < 0 || screenX >= static_cast<int32_t>(common::GAMEBOY_WIDTH)) continue;

        // getPixelFromLine takes a pixel index (0 = leftmost) and handles the
        // bit order itself, so only X-flipped sprites reverse the column
        const uint32_t col = static_cast<uint32_t>(flipX ? (ppu::TILE_WIDTH_PX - 1 - px) : px);
        const GBColor gbColor = getPixelFromLine(byte1, byte2, col);
        if (gbColor == GBColor::Color0) continue; // transparent

        const uint32_t idx = static_cast<uint32_t>(line) * common::GAMEBOY_WIDTH + static_cast<uint32_t>(screenX);
        const uint8_t cache = colorCache[idx];
        const bool bgColorNonzero = (cache & 0x03) != 0;
        const uint8_t shade = static_cast<uint8_t>(gbColor);

        if (emulator.cgbMode)
        {
            // CGB priority: BG tile priority bit (bit 2 of cache) blocks sprites
            // when LCDC bit 0 is set (bgEnabled). When LCDC bit 0 is clear,
            // sprites always render above BG/Window regardless.
            const bool bgHasPriority = checkBit(cache, 2); // set by drawBGLine/drawWindowLine
            if (bgEnabled() && (objectBehindBG || bgHasPriority) && bgColorNonzero) continue;
            buffer.setPixelRGB(static_cast<uint32_t>(screenX), static_cast<uint32_t>(line),
                objColorPalettes.getColor(cgbPal, shade), shade);
        }
        else
        {
            if (objectBehindBG && bgColorNonzero) continue;
            if (emulator.sgbMode)
            {
                const uint8_t mappedShade = static_cast<uint8_t>(getColorFromPalette(gbColor, palette));
                const uint8_t paletteIdx  = emulator.sgb.getAttribute(static_cast<uint32_t>(screenX), static_cast<uint32_t>(line));
                buffer.setPixelRGB(static_cast<uint32_t>(screenX), static_cast<uint32_t>(line),
                    emulator.sgb.getColor(paletteIdx, mappedShade, static_cast<uint32_t>(screenX), static_cast<uint32_t>(line)), mappedShade);
            }
            else
                buffer.setPixel(static_cast<uint32_t>(screenX), static_cast<uint32_t>(line), getColorFromPalette(gbColor, palette));
        }

        // Mark that a sprite pixel was placed here (bit 3)
        colorCache[idx] = setBit(cache, 3);
    }
}

GBColor PPU::getPixelFromLine(uint8_t byteOne, uint8_t byteTwo, uint8_t pixelIndex)
{
    // Bit 7 holds the leftmost pixel. The second byte
    // supplies the high bit of each pixel's color number.
    const uint8_t bit = static_cast<uint8_t>((ppu::TILE_WIDTH_PX - 1) - pixelIndex);
    uint8_t color = composeBits(getBitValue(byteTwo, bit), getBitValue(byteOne, bit));
    return getColor(color);
}

bool PPU::isOnScreenX(int32_t x)
{
    return (x >= 0) && (x < common::GAMEBOY_WIDTH);
}

bool PPU::isOnScreenY(int32_t y)
{
    return (y >= 0) && (y < common::GAMEBOY_HEIGHT);
}

bool PPU::isOnScreen(int32_t x, int32_t y)
{
    return isOnScreenX(x) && isOnScreenY(y);
}

ColorPalette PPU::loadPalette(ByteRegister &paletteRegister)
{
    uint8_t color0 = composeBits(getBitValue(paletteRegister.getValue(), 1), getBitValue(paletteRegister.getValue(), 0));
    uint8_t color1 = composeBits(getBitValue(paletteRegister.getValue(), 3), getBitValue(paletteRegister.getValue(), 2));
    uint8_t color2 = composeBits(getBitValue(paletteRegister.getValue(), 5), getBitValue(paletteRegister.getValue(), 4));
    uint8_t color3 = composeBits(getBitValue(paletteRegister.getValue(), 7), getBitValue(paletteRegister.getValue(), 6));

    Color realColor0 = getRealColor(color0);
    Color realColor1 = getRealColor(color1);
    Color realColor2 = getRealColor(color2);
    Color realColor3 = getRealColor(color3);

    return { realColor0, realColor1, realColor2, realColor3 };
}

Color PPU::getColorFromPalette(GBColor color, const ColorPalette &palette) const
{
    switch (color)
    {
        case GBColor::Color0: return palette.color0;
        case GBColor::Color1: return palette.color1;
        case GBColor::Color2: return palette.color2;
        case GBColor::Color3: return palette.color3;
    }
    return palette.color0;
}

Color PPU::getRealColor(uint8_t pixelValue)
{
    switch (pixelValue)
    {
        case 0: return Color::White;
        case 1: return Color::LightGray;
        case 2: return Color::DarkGray;
        case 3: return Color::Black;
        default: throw std::runtime_error("Invalid color");
    }
    return Color::White;
}

void PPU::registerVBlankCallback(const std::function<void(const FrameBuffer&)> &vblankCallback)
{
    this->vblankCallback = vblankCallback;
}

void PPU::draw()
{
    vblankCallback(buffer);
}
