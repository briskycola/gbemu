#include "Core/Video/PPU.hpp"
#include "Core/Video/Color.hpp"
#include "Core/Emulator.hpp"
#include "Core/Common/Common.hpp"
#include "Core/Memory/IORegisters.hpp"
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
        case io::BG_PALETTE_DATA_REGISTER:   bgColorPalettes.writeData(value, cpuCanAccessVRAM()); break;
        case io::OBJ_PALETTE_INDEX_REGISTER: objColorPalettes.writeIndex(value); break;
        default:     objColorPalettes.writeData(value, cpuCanAccessVRAM()); break;
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
    if (checkBit(stat, ppu::STAT_VBLANK_INTERRUPT_BIT) && currentMode == VideoMode::VBlank && lateInterruptDelay == 0)
        signal = setBit(signal, ppu::IRQ48_VBLANK_BIT);
    if (checkBit(stat, ppu::STAT_OAM_INTERRUPT_BIT) && ((currentMode == VideoMode::AccessOAM && lateInterruptDelay == 0) || line144OAMPulse))
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
    if (!emulator.cgbHardware)
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

// At the start of each line the window's Y condition becomes true if WY equals
// LY, whether or not the window is enabled, and stays true until VBlank
// https://gbdev.io/pandocs/Window.html#window-rendering-criteria
void PPU::checkWindowYTrigger()
{
    if (!windowYTriggered && line.getValue() == windowY.getValue())
    {
        windowYTriggered = true;
    }
}

// The OAM scan picks the first 10 objects in OAM order that are on this line,
// whatever their X. They are fetched from left to right, and objects with the
// same X in OAM order.
// https://gbdev.io/pandocs/OAM.html#selection-priority
void PPU::scanOAM()
{
    const int32_t height = usesLargeSprites() ? 16 : 8;
    const int32_t currentLine = line.getValue();
    lineObjectCount = 0;
    for (uint8_t i = 0; i < ppu::MAX_SPRITES && lineObjectCount < ppu::MAX_SPRITES_PER_LINE; ++i)
    {
        const int32_t top = static_cast<int32_t>(emulator.mmu.read(Address(mmu::OAM_ADDRESS_START + i * ppu::SPRITE_BYTES + ppu::OAM_SPRITE_Y_BYTE), true)) - ppu::SPRITE_Y_OFFSET;
        if (currentLine >= top && currentLine < top + height) lineObjects[lineObjectCount++] = i;
    }
    const auto objectX = [this](uint8_t index)
    {
        return emulator.mmu.read(Address(mmu::OAM_ADDRESS_START + index * ppu::SPRITE_BYTES + ppu::OAM_SPRITE_X_BYTE), true);
    };
    std::stable_sort(lineObjects.begin(), lineObjects.begin() + lineObjectCount,
        [&](uint8_t a, uint8_t b) { return objectX(a) < objectX(b); });
}

// Starts drawing a line. At the start of mode 3 both FIFOs are cleared, and
// the fetcher starts on the first tile of the background.
// https://gbdev.io/pandocs/pixel_fifo.html#mode-3-operation
void PPU::startMode3(uint32_t length, uint32_t start)
{
    lineLength = length;
    mode3Start = start;
    scanOAM();

    bgFifoHead = 0;
    bgFifoSize = 0;
    objFifoSize = 0;
    fetchStep = FetchStep::Tile;
    fetchStepDots = 0;
    firstFetch = true;
    fetchingWindow = false;
    fetcherIdleDots = 0;
    fetcherStarting = true;
    fetcherX = 0;

    mode3Dots = 0;
    pixelX = 0;
    // The pixels left of SCX's fine scroll are shifted out and dropped,
    // a dot each
    // https://gbdev.io/pandocs/Rendering.html#mode-3-length
    fineScrollShifted = 0;
    fineScrollDone = false;
    windowPixelsToShift = 0;
    windowPixelsToDrop = 0;
    objectStall = 0;
    nextLineObject = 0;
    tilesWaitedForCount = 0;
    insertBlankPixel = false;

    // The DMG's WX = 166 window, carried on from the line before, starting
    // at its second tile
    if (windowFromLineStart)
    {
        windowFromLineStart = false;
        fetchingWindow = true;
        windowRow = nextLineWindowRow;
        windowStartX = -static_cast<int32_t>(ppu::TILE_WIDTH_PX);
        windowStartWX = windowX.getValue();
        fetcherX = 1;
    }

    setMode(VideoMode::AccessVRAM);
}

// Runs mode 3 up to the current dot. When the 160th pixel has been sent to the
// LCD, mode 0 starts and gets the rest of the line.
void PPU::runMode3()
{
    while (mode3Dots < cycleCounter)
    {
        if (!stepMode3()) continue;

        cycleCounter -= mode3Dots;
        hblankLength = lineLength - mode3Start - mode3Dots;

        // On the DMG, a window at WX = 166 isn't drawn on this line but
        // carries on into the next one from its left edge
        // https://gbdev.io/pandocs/Window.html#window-rendering-criteria
        windowFromLineStart = !emulator.cgbHardware && windowEnabled() && windowYTriggered &&
            windowX.getValue() == ppu::DMG_WX_BUG_VALUE;
        if (windowFromLineStart) nextLineWindowRow = static_cast<uint8_t>(windowLine++);
        setMode(VideoMode::HBlank);

        // Copy one HBlank DMA block. Only the CPU stalls while it runs
        // (through dmaCyclesRemaining); the PPU keeps its own timing.
        emulator.mmu.hblankDMA();

        // The rest of this tick is mode 0
        advanceLCD(cycleCounter);

        refreshStatSignal(true);
        return;
    }
}

// One dot of mode 3.
// Returns true once the line's last pixel has been sent.
bool PPU::stepMode3()
{
    ++mode3Dots;
    advanceLCD(1);

    if (objectStall > 0)
    {
        // The object's row is read on the fetch's last dot, with the object
        // size LCDC.2 has then (Mealybug Tearoom's captures)
        if (--objectStall == 0)
            mixObjectIntoFifo(fetchingObject, readObjectRow(fetchingObject, false), readObjectRow(fetchingObject, true));
        stepFetcher();
        return false;
    }

    // The fetcher runs first, then a pixel may be sent to the LCD. On the dot
    // the fetcher (re)starts, at the start of the line or of the window, it
    // doesn't run yet.
    const bool windowStarted = startWindowIfReached();
    if (fetcherStarting) fetcherStarting = false;
    else stepFetcher();

    if (windowStarted || bgFifoSize == 0 || fetchObjectIfReached()) return false;

    // If WX changes after the window has started and the pixel counter
    // reaches the new value, a color 0 pixel goes out ahead of the FIFO.
    // This only happens on a tile boundary, just after a window tile past
    // the first has been pushed (Mealybug Tearoom's m3_wx_*_change
    // captures, and SameBoy; Pan Docs doesn't give the condition).
    // https://gbdev.io/pandocs/pixel_fifo.html#the-window
    const bool tileJustPushed = fetchStep == FetchStep::Tile && fetchStepDots == 0 && bgFifoSize == ppu::TILE_WIDTH_PX;
    if (fetchingWindow && tileJustPushed && static_cast<int32_t>(pixelX) > windowStartX &&
        windowX.getValue() != windowStartWX && pixelsStillToDrop() == 0 &&
        pixelX + ppu::WINDOW_X_OFFSET == windowX.getValue())
        insertBlankPixel = true;
    popPixel();
    return pixelX == common::GAMEBOY_WIDTH;
}


// Pixels still to be shifted out before the line's first pixel is drawn
uint8_t PPU::pixelsStillToDrop() const
{
    const uint8_t fineScroll = scrollX.getValue() % ppu::TILE_WIDTH_PX;
    const uint8_t scrollLeft = fineScrollDone || fineScrollShifted >= fineScroll ? 0 : static_cast<uint8_t>(fineScroll - fineScrollShifted);
    return static_cast<uint8_t>(windowPixelsToShift + scrollLeft);
}
// The window starts when the pixel counter, which runs 7 ahead of the pixels
// sent to the LCD, equals WX while the window is enabled and its Y condition
// is true. The background FIFO is cleared and the fetcher starts again on the
// window's current row. WX below 7 puts the window's first pixels left of the
// screen; WX = 0 also starts it before SCX's fine scroll, which then shifts
// the window too.
// https://gbdev.io/pandocs/Window.html#window-rendering-criteria
// https://gbdev.io/pandocs/pixel_fifo.html#the-window
bool PPU::startWindowIfReached()
{
    if (fetchingWindow || !windowEnabled() || !windowYTriggered) return false;

    const uint8_t wx = windowX.getValue();
    if (wx == ppu::DMG_WX_BUG_VALUE && !emulator.cgbHardware) return false;
    if (wx >= ppu::WINDOW_X_OFFSET)
    {
        if (pixelsStillToDrop() > 0 || pixelX + ppu::WINDOW_X_OFFSET != wx) return false;
    }
    else if (pixelX != 0 || (wx > 0 && pixelsStillToDrop() > 0))
    {
        return false;
    }

    fetchingWindow = true;
    windowStartX = static_cast<int32_t>(wx) - static_cast<int32_t>(ppu::WINDOW_X_OFFSET);
    windowStartWX = wx;
    windowPixelsToDrop = wx < ppu::WINDOW_X_OFFSET ? static_cast<uint8_t>(ppu::WINDOW_X_OFFSET - wx) : 0;
    windowRow = static_cast<uint8_t>(windowLine++);

    // At the start of the line the window is decided on at once, but the
    // line's first fetch still runs, and so does the fetch of the first
    // background tile, which is wasted. So the window's first pixel comes 6
    // dots later than the background's would have, as anywhere else. (Pan
    // Docs gives the 6 dots; when the window is decided on follows Mealybug
    // Tearoom's m2_win_en_toggle and m3_window_timing captures.)
    // With WX below 7 the window's pixels left of the screen are shifted
    // out a dot each, and the window starts that much sooner, so its first
    // pixel on screen still comes at the same time.
    if (pixelX == 0 && bgFifoSize == 0)
    {
        // At WX = 0 that is 7 pixels, a dot more than the wasted fetch, so
        // the line's first fetch ends a dot sooner instead
        const int32_t idle = static_cast<int32_t>(ppu::TILE_FETCH_DOTS) - windowPixelsToDrop;
        fetchStep = FetchStep::Tile;
        fetchStepDots = idle < 0 ? 1 : 0;
        fetcherIdleDots = static_cast<uint8_t>(std::max(0, idle));
        windowPixelsToShift = windowPixelsToDrop;
        windowPixelsToDrop = 0;
        // On the DMG, WX = 0 with a fine scroll costs a dot more (Mealybug
        // Tearoom's m3_window_timing_wx_0 captures, and SameBoy; Pan Docs
        // instead has mode 3 a dot shorter in this case)
        if (wx == 0 && scrollX.getValue() % ppu::TILE_WIDTH_PX != 0 && !emulator.cgbHardware) ++fetcherIdleDots;
        return true;
    }

    bgFifoHead = 0;
    bgFifoSize = 0;
    fetchStep = FetchStep::Tile;
    fetchStepDots = 0;
    firstFetch = false;
    fetcherStarting = true;
    fetcherX = 0;
    return true;
}

// The fetcher's steps. Get Tile reads the tile number (and on the CGB its
// attributes) from the tile map, the two Get Tile Data steps read the tile's
// row, and Push puts the row into the background FIFO once it is empty. Get
// Tile Data High also tries to push.
// https://gbdev.io/pandocs/pixel_fifo.html#fifo-pixel-fetcher
void PPU::stepFetcher()
{
    if (fetcherIdleDots > 0)
    {
        --fetcherIdleDots;
        return;
    }

    if (fetchStep == FetchStep::Push)
    {
        pushTileRow();
        return;
    }

    // Each step takes 2 dots, and does its work on the second
    if (++fetchStepDots < ppu::FETCH_STEP_DOTS) return;
    fetchStepDots = 0;

    // The VRAM address, from the registers as they are now
    {
        switch (fetchStep)
        {
            case FetchStep::Tile:
            {
                // Clearing LCDC.5 sends the fetcher back to the background
                if (fetchingWindow && !windowEnabled()) fetchingWindow = false;

                // The window uses the LCDC.6 tile map and its own row and
                // column. The background uses the LCDC.3 tile map, at the
                // column of the next pixel to be pushed (Pan Docs'
                // ((SCX / 8) + fetcher X) & 31 while SCX doesn't change) and
                // row (LY + SCY) & 255.
                // https://gbdev.io/pandocs/pixel_fifo.html#get-tile
                const bool highMap = fetchingWindow ? usesWindowTileMap() : usesBackgroundTileMap();
                const uint16_t mapBase = (highMap ? ppu::TILE_MAP_ONE_ADDRESS : ppu::TILE_MAP_ZERO_ADDRESS).getValue() - mmu::VRAM_ADDRESS_START;
                const int32_t nextPixel = static_cast<int32_t>(pixelX) + bgFifoSize - pixelsStillToDrop();
                const uint8_t tileX = fetchingWindow
                    ? static_cast<uint8_t>(fetcherX & (ppu::TILES_PER_LINE - 1))
                    : static_cast<uint8_t>(((scrollX.getValue() + nextPixel) / static_cast<int32_t>(ppu::TILE_WIDTH_PX)) & (ppu::TILES_PER_LINE - 1));
                const uint8_t y = fetchingWindow ? windowRow : static_cast<uint8_t>(line.getValue() + scrollY.getValue());
                fetchAddress = static_cast<uint16_t>(mapBase + (y / ppu::TILE_HEIGHT_PX) * ppu::TILES_PER_LINE + tileX);
                break;
            }
            case FetchStep::DataLow:
                fetchAddress = tileDataAddress(fetchedTile, fetchedAttributes, false);
                break;
            case FetchStep::DataHigh:
                fetchAddress = tileDataAddress(fetchedTile, fetchedAttributes, true);
                break;
            case FetchStep::Push:
                break;
        }
    }

    const uint8_t bank = getBitValue(fetchedAttributes, ppu::ATTRIBUTE_VRAM_BANK_BIT);
    switch (fetchStep)
    {
        case FetchStep::Tile:
            // Tile numbers live in VRAM bank 0 and attributes in bank 1, and
            // the CGB reads both at once
            fetchedTile = readVRAMBank(fetchAddress, 0);
            fetchedAttributes = emulator.cgbMode ? readVRAMBank(fetchAddress, 1) : 0;
            fetchStep = FetchStep::DataLow;
            break;
        case FetchStep::DataLow:
            fetchedLow = readVRAMBank(fetchAddress, bank);
            fetchStep = FetchStep::DataHigh;
            break;
        case FetchStep::DataHigh:
            fetchedHigh = readVRAMBank(fetchAddress, bank);
            fetchStep = FetchStep::Push;
            pushTileRow();
            break;
        case FetchStep::Push:
            break;
    }
}

// LCDC.4 picks the tile data area: $8000 with unsigned tile numbers, or $8800
// with signed ones based at $9000. The row is (LY + SCY) % 8 for the
// background, read again at each step, or the window row; on the CGB it can
// be flipped vertically.
// https://gbdev.io/pandocs/pixel_fifo.html#get-tile-data-low
// https://gbdev.io/pandocs/Tile_Data.html
uint16_t PPU::tileDataAddress(uint8_t tile, uint8_t attributes, bool highByte) const
{
    uint8_t row = static_cast<uint8_t>((fetchingWindow ? windowRow : line.getValue() + scrollY.getValue()) % ppu::TILE_HEIGHT_PX);
    if (checkBit(attributes, ppu::ATTRIBUTE_Y_FLIP_BIT)) row = static_cast<uint8_t>(ppu::TILE_HEIGHT_PX - 1 - row);
    const uint16_t tileBase = bgWindowTileData()
        ? static_cast<uint16_t>(tile * ppu::TILE_BYTES)
        : static_cast<uint16_t>(ppu::SIGNED_TILE_DATA_BASE + static_cast<int8_t>(tile) * static_cast<int32_t>(ppu::TILE_BYTES));
    return static_cast<uint16_t>(tileBase + row * ppu::TILE_BYTES_PER_ROW + (highByte ? 1 : 0));
}

// Pixels are only pushed when the background FIFO is empty: the row's 8
// pixels, leftmost first, or rightmost first when the tile is flipped
// horizontally. The line's first fetch is thrown away and the first tile
// fetched again, which is part of mode 3's 172 dots.
// https://gbdev.io/pandocs/pixel_fifo.html#push
bool PPU::pushTileRow()
{
    if (bgFifoSize > 0) return false;
    fetchStep = FetchStep::Tile;
    fetchStepDots = 0;

    if (firstFetch)
    {
        firstFetch = false;
        return false;
    }

    // On the DMG, if the window is disabled but would otherwise start right
    // here, on a tile boundary, a single color 0 pixel goes in first, which
    // pushes the rest of the line one pixel right
    // https://gbdev.io/pandocs/Window.html#window-rendering-criteria
    if (!emulator.cgbHardware && !fetchingWindow && !windowEnabled() && windowYTriggered &&
        pixelsStillToDrop() == 0 && pixelX + ppu::WINDOW_X_OFFSET == windowX.getValue())
    {
        bgFifo[(bgFifoHead + bgFifoSize++) % ppu::FIFO_CAPACITY] = FifoPixel{};
        fetchStep = FetchStep::Push;
        return true;
    }

    const bool flipX = checkBit(fetchedAttributes, ppu::ATTRIBUTE_X_FLIP_BIT);
    for (uint8_t i = 0; i < ppu::TILE_WIDTH_PX; ++i)
    {
        // A window starting left of the screen loses its leftmost pixels
        if (windowPixelsToDrop > 0) { --windowPixelsToDrop; continue; }

        const uint8_t bit = static_cast<uint8_t>(flipX ? i : ppu::TILE_WIDTH_PX - 1 - i);
        FifoPixel &pixel = bgFifo[(bgFifoHead + bgFifoSize++) % ppu::FIFO_CAPACITY];
        pixel.colorIndex = tileColorIndex(fetchedLow, fetchedHigh, bit);
        pixel.palette = fetchedAttributes & ppu::ATTRIBUTE_CGB_PALETTE_MASK;
        pixel.bgPriority = checkBit(fetchedAttributes, ppu::ATTRIBUTE_PRIORITY_BIT);
    }
    ++fetcherX;
    return true;
}

// When the next pixel has an object on it, the object is fetched before the
// pixel is sent. Objects are fetched only while LCDC.1 is set, except on the
// CGB. Objects partly left of the screen are fetched at the first pixel, and
// objects at OAM X 168 or more never are.
// https://gbdev.io/pandocs/pixel_fifo.html#sprites
bool PPU::fetchObjectIfReached()
{
    if (pixelsStillToDrop() > 0) return false;

    while (nextLineObject < lineObjectCount)
    {
        const uint8_t index = lineObjects[nextLineObject];
        const uint8_t oamX = emulator.mmu.read(Address(mmu::OAM_ADDRESS_START + index * ppu::SPRITE_BYTES + ppu::OAM_SPRITE_X_BYTE), true);
        if (oamX >= common::GAMEBOY_WIDTH + ppu::SPRITE_X_OFFSET || oamX > pixelX + ppu::SPRITE_X_OFFSET) return false;

        ++nextLineObject;
        if (!spritesEnabled() && !emulator.cgbHardware) continue;

        // This dot is the first of the fetch
        fetchingObject = index;
        objectStall = objectFetchDots(oamX) - 1;
        return true;
    }
    return false;
}

// How long an object fetch holds back the pixel output, by Pan Docs' OBJ
// penalty algorithm: 6 dots, plus, for the first object starting in a given
// background or window tile, the pixels of that tile to the right of the
// object minus 2. An object at OAM X = 0 is left of the screen in a tile of
// its own whatever SCX is, which costs 11 dots for the first; others at X = 0
// share that tile, as mooneye's intr_2_mode0_timing_sprites shows.
// https://gbdev.io/pandocs/Rendering.html#obj-penalty-algorithm
uint32_t PPU::objectFetchDots(uint8_t oamX)
{
    int32_t tile = ppu::OBJ_X0_TILE;
    int32_t pixelsToTheRight = static_cast<int32_t>(ppu::TILE_WIDTH_PX) - 1;
    if (oamX != 0)
    {
        const int32_t pixel = static_cast<int32_t>(oamX) - ppu::SPRITE_X_OFFSET;
        const bool inWindow = fetchingWindow && pixel >= windowStartX;
        const int32_t coordinate = inWindow ? pixel - windowStartX : (pixel + scrollX.getValue()) & (common::BACKGROUND_MAP_SIZE - 1);
        tile = (inWindow ? ppu::OBJ_WINDOW_TILE_BASE : 0) + coordinate / static_cast<int32_t>(ppu::TILE_WIDTH_PX);
        pixelsToTheRight = static_cast<int32_t>(ppu::TILE_WIDTH_PX) - 1 - coordinate % static_cast<int32_t>(ppu::TILE_WIDTH_PX);
    }

    uint32_t dots = ppu::OBJ_FETCH_PENALTY;
    if (std::find(tilesWaitedFor.begin(), tilesWaitedFor.begin() + tilesWaitedForCount, tile) == tilesWaitedFor.begin() + tilesWaitedForCount)
    {
        tilesWaitedFor[tilesWaitedForCount++] = tile;
        dots += static_cast<uint32_t>(std::max(0, pixelsToTheRight - static_cast<int32_t>(ppu::OBJ_BG_FETCH_ALLOWANCE)));
    }
    return dots;
}

// Fetches an object's row and mixes it into the object FIFO, which is first
// filled up to 8 pixels with transparent ones. A pixel already in the FIFO
// is replaced only if it is transparent, or on the CGB in OAM priority mode
// if it belongs to an object later in OAM. (On the DMG the pixels already
// there are from objects with a lower X, or an equal X and earlier in OAM,
// which win.) On the CGB the tile can come from VRAM bank 1 and be flipped.
// https://gbdev.io/pandocs/pixel_fifo.html#sprites
// https://gbdev.io/pandocs/OAM.html#drawing-priority
// Reads one byte of an object's row, with the object size LCDC.2 has now
uint8_t PPU::readObjectRow(uint8_t oamIndex, bool highByte)
{
    const uint16_t oamBase = mmu::OAM_ADDRESS_START + oamIndex * ppu::SPRITE_BYTES;
    const uint8_t oamY = emulator.mmu.read(Address(oamBase + ppu::OAM_SPRITE_Y_BYTE), true);
    uint8_t tile = emulator.mmu.read(Address(oamBase + ppu::OAM_SPRITE_TILE_BYTE), true);
    const uint8_t attributes = emulator.mmu.read(Address(oamBase + ppu::OAM_SPRITE_ATTRIBUTE_BYTE), true);

    // In 8x16 mode bit 0 of the tile number is ignored
    // https://gbdev.io/pandocs/OAM.html#byte-2--tile-index
    const bool tall = usesLargeSprites();
    const uint8_t heightMask = tall ? 15 : 7;
    if (tall) tile &= 0xFE;
    uint8_t row = static_cast<uint8_t>((line.getValue() + ppu::SPRITE_Y_OFFSET - oamY) & heightMask);
    if (checkBit(attributes, ppu::ATTRIBUTE_Y_FLIP_BIT)) row = static_cast<uint8_t>(heightMask - row);

    const uint8_t bank = emulator.cgbMode ? getBitValue(attributes, ppu::ATTRIBUTE_VRAM_BANK_BIT) : 0;
    const uint16_t address = static_cast<uint16_t>(tile * ppu::TILE_BYTES + row * ppu::TILE_BYTES_PER_ROW);
    return readVRAMBank(static_cast<uint16_t>(address + (highByte ? 1 : 0)), bank);
}

void PPU::mixObjectIntoFifo(uint8_t oamIndex, uint8_t low, uint8_t high)
{
    const uint16_t oamBase = mmu::OAM_ADDRESS_START + oamIndex * ppu::SPRITE_BYTES;
    const uint8_t oamX = emulator.mmu.read(Address(oamBase + ppu::OAM_SPRITE_X_BYTE), true);
    const uint8_t attributes = emulator.mmu.read(Address(oamBase + ppu::OAM_SPRITE_ATTRIBUTE_BYTE), true);

    while (objFifoSize < ppu::OBJ_FIFO_PIXELS) objFifo[objFifoSize++] = FifoPixel{};

    const bool flipX = checkBit(attributes, ppu::ATTRIBUTE_X_FLIP_BIT);
    const bool oamOrder = emulator.cgbHardware && objectPriorityMode == 0;
    const int32_t left = static_cast<int32_t>(oamX) - ppu::SPRITE_X_OFFSET;
    for (int32_t column = 0; column < static_cast<int32_t>(ppu::TILE_WIDTH_PX); ++column)
    {
        const int32_t slot = left + column - pixelX;
        if (slot < 0 || slot >= ppu::OBJ_FIFO_PIXELS) continue;

        const uint8_t bit = static_cast<uint8_t>(flipX ? column : ppu::TILE_WIDTH_PX - 1 - column);
        const uint8_t colorIndex = tileColorIndex(low, high, bit);
        FifoPixel &existing = objFifo[slot];
        if (colorIndex == 0) continue;
        if (existing.colorIndex != 0 && !(oamOrder && existing.oamIndex > oamIndex)) continue;

        existing.colorIndex = colorIndex;
        existing.palette = emulator.cgbMode
            ? attributes & ppu::ATTRIBUTE_CGB_PALETTE_MASK
            : getBitValue(attributes, ppu::ATTRIBUTE_DMG_PALETTE_BIT);
        existing.bgPriority = checkBit(attributes, ppu::ATTRIBUTE_PRIORITY_BIT);
        existing.oamIndex = oamIndex;
    }
}

// Pops a pixel off each FIFO and sends the pair towards the LCD
// https://gbdev.io/pandocs/pixel_fifo.html#pixel-rendering
void PPU::popPixel()
{
    // The WX-change bug's pixel is drawn in place of the FIFO's next one
    FifoPixel bg;
    if (insertBlankPixel)
    {
        insertBlankPixel = false;
    }
    else
    {
        bg = bgFifo[bgFifoHead];
        bgFifoHead = static_cast<uint8_t>((bgFifoHead + 1) % ppu::FIFO_CAPACITY);
        --bgFifoSize;
    }

    // Window pixels left of the screen, then SCX's fine scroll, are shifted
    // out without being drawn. The fine scroll ends once SCX % 8 pixels have
    // gone, with SCX as it is while they go (Mealybug Tearoom's captures).
    if (windowPixelsToShift > 0)
    {
        --windowPixelsToShift;
        return;
    }
    if (!fineScrollDone)
    {
        if (fineScrollShifted < scrollX.getValue() % ppu::TILE_WIDTH_PX)
        {
            ++fineScrollShifted;
            return;
        }
        fineScrollDone = true;
    }

    FifoPixel object;
    if (objFifoSize > 0)
    {
        object = objFifo[0];
        std::copy(objFifo.begin() + 1, objFifo.begin() + objFifoSize, objFifo.begin());
        --objFifoSize;
    }

    const LCDPixel pixel{bg, object, pixelX++, ppu::LCD_OUTPUT_LATENCY};
    if (pixel.dotsLeft == 0)
    {
        drawPixel(pixel);
        return;
    }
    lcdPipeline[(lcdPipelineHead + lcdPipelineSize++) % ppu::LCD_PIPELINE_CAPACITY] = pixel;
}

// Moves the pixels on their way to the LCD along by some dots, drawing those
// that arrive. Each dot also ends a DMG palette write's glitch dot.
void PPU::advanceLCD(uint32_t dots)
{
    for (uint32_t dot = 0; dot < dots && (lcdPipelineSize > 0 || paletteGlitchDots > 0 || outputControlDots > 0); ++dot)
    {
        for (uint8_t i = 0; i < lcdPipelineSize; ++i) --lcdPipeline[(lcdPipelineHead + i) % ppu::LCD_PIPELINE_CAPACITY].dotsLeft;
        while (lcdPipelineSize > 0 && lcdPipeline[lcdPipelineHead].dotsLeft == 0)
        {
            drawPixel(lcdPipeline[lcdPipelineHead]);
            lcdPipelineHead = static_cast<uint8_t>((lcdPipelineHead + 1) % ppu::LCD_PIPELINE_CAPACITY);
            --lcdPipelineSize;
        }
        if (paletteGlitchDots > 0 && --paletteGlitchDots == 0) finishPaletteWrite();
        if (outputControlDots > 0 && --outputControlDots == 0) outputControl = pendingOutputControl;
    }
}

// Mixes a background and an object pixel and draws the result. The palettes
// and LCDC bits 0 and 1 are read now, as the pixel reaches the LCD.
// https://gbdev.io/pandocs/pixel_fifo.html#pixel-rendering
void PPU::drawPixel(const LCDPixel &pixel)
{
    const FifoPixel &bg = pixel.bg;
    const FifoPixel &object = pixel.object;

    // On the DMG, LCDC.0 clear makes the background and window white. On the
    // CGB it instead takes away their priority over objects.
    // https://gbdev.io/pandocs/LCDC.html#lcdc0--bg-and-window-enablepriority
    const bool bgOn = checkBit(outputControl, ppu::LCDC_BG_ENABLE_BIT);
    const bool bgShown = emulator.cgbMode || bgOn;
    const uint8_t bgColorIndex = bgShown ? bg.colorIndex : 0;

    // An object pixel is drawn unless it is transparent (color index 0),
    // objects are off, or the background pixel is not color index 0 and has
    // priority: through the object's OBJ-to-BG bit, or on the CGB the tile's
    // own priority bit.
    // https://gbdev.io/pandocs/Tile_Maps.html#bg-to-obj-priority-in-cgb-mode
    bool objectWins = object.colorIndex != 0 && checkBit(outputControl, ppu::LCDC_SPRITES_ENABLE_BIT);
    if (objectWins && bgColorIndex != 0)
    {
        const bool bgPriority = emulator.cgbMode ? bgOn && (object.bgPriority || bg.bgPriority) : object.bgPriority;
        if (bgPriority) objectWins = false;
    }
    const uint8_t colorIndex = objectWins ? object.colorIndex : bgColorIndex;

    const uint32_t x = pixel.x;
    const uint32_t y = line.getValue();

    // In CGB Mode the color index picks a color of the pixel's CGB palette
    // https://gbdev.io/pandocs/Palettes.html#lcd-color-palettes-cgb-only
    if (emulator.cgbMode)
    {
        const uint32_t rgb = objectWins ? objColorPalettes.getColor(object.palette, colorIndex) : bgColorPalettes.getColor(bg.palette, colorIndex);
        buffer.setPixelRGB(x, y, rgb);
        return;
    }

    // Otherwise BGP, OBP0 or OBP1 assigns the color index a shade. A hidden
    // background is white.
    const ByteRegister &palette = objectWins ? (object.palette ? spritePalette1 : spritePalette0) : bgPalette;
    const Shade shade = !objectWins && !bgShown ? Shade::White : shadeFromPalette(palette.getValue(), colorIndex);

    // In the Game Boy Color's DMG compatibility mode, the shade picks a color
    // of CGB BG palette 0, or OBJ palette 0 or 1, instead of a gray
    // https://gbdev.io/pandocs/Power_Up_Sequence.html#compatibility-palettes
    if (emulator.cgbHardware)
    {
        const uint8_t index = shadeIndex(shade);
        const uint32_t rgb = objectWins ? objColorPalettes.getColor(object.palette, index) : bgColorPalettes.getColor(0, index);
        buffer.setPixel(x, y, shade, rgb);
        return;
    }

    // The Super Game Boy colors the shade from the palette of the pixel's
    // attribute area
    if (emulator.sgbMode)
    {
        buffer.setPixel(x, y, shade, emulator.sgb.getColor(emulator.sgb.getAttribute(x, y), shade, x, y));
        return;
    }

    buffer.setPixel(x, y, shade);
}

// BGP, OBP0 and OBP1. On the DMG (not the SGB or CGB) the LCD sees the old
// value ORed with the new one for a dot before the new one.
void PPU::writeDMGPalette(ByteRegister &palette, uint8_t value)
{
    if (paletteGlitchDots > 0) finishPaletteWrite();
    if (emulator.cgbHardware || emulator.sgbMode || !displayEnabled())
    {
        palette.set(value);
        return;
    }
    glitchedPalette = &palette;
    glitchedPaletteValue = value;
    paletteGlitchDots = ppu::PALETTE_GLITCH_DOTS;
    palette.set(palette.getValue() | value);
}

void PPU::finishPaletteWrite()
{
    paletteGlitchDots = 0;
    if (glitchedPalette) glitchedPalette->set(glitchedPaletteValue);
    glitchedPalette = nullptr;
}


// At line 144 the VBlank interrupt is requested and the mode 1 STAT source
// rises. On the DMG, line 144 also briefly raises the mode 2 STAT source, so
// the mode 2 STAT interrupt fires too (mooneye's vblank_stat_intr-GS; the CGB
// doesn't). At line 0 the mode 2 STAT source rises.
// https://gbdev.io/pandocs/Interrupt_Sources.html#int-40--vblank-interrupt
void PPU::raiseLateInterrupts()
{
    if (currentMode != VideoMode::VBlank)
    {
        refreshStatSignal(true);
        return;
    }
    emulator.cpu.interruptFlag.setBitTo(cpu::INTERRUPT_VBLANK_BIT, true);
    line144OAMPulse = !emulator.cgbHardware; // The CGB's pulse came as the line started
    refreshStatSignal(true);
    line144OAMPulse = false;
    refreshStatSignal(true);
}

void PPU::tickAhead(uint32_t dots)
{
    tick(Cycles(dots));
    dotsAhead += dots;
}

uint32_t PPU::registerWriteDot(uint16_t address, uint8_t value) const
{
    switch (address)
    {
        // Switching the LCD on or off keeps the usual timing
        case io::LCD_CONTROL_REGISTER:
            if (checkBit(value, ppu::LCDC_DISPLAY_ENABLE_BIT) != displayEnabled()) break;
            return ppu::RENDER_REGISTER_WRITE_DOT;
        case io::SCROLL_Y_REGISTER:
        case io::SCROLL_X_REGISTER:
        case io::BG_PALETTE_REGISTER:
        case io::OBJ_PALETTE0_REGISTER:
        case io::OBJ_PALETTE1_REGISTER:
        case io::WINDOW_X_REGISTER:
            return ppu::RENDER_REGISTER_WRITE_DOT;
        default:
            break;
    }
    return common::T_CYCLES_PER_M_CYCLE;
}
void PPU::tick(Cycles cycles)
{
    // Skip the dots tickAhead() already ran
    if (dotsAhead > 0)
    {
        const uint32_t skipped = std::min(dotsAhead, cycles.cycles);
        dotsAhead -= skipped;
        if (cycles.cycles > skipped) tick(Cycles(cycles.cycles - skipped));
        return;
    }

    if (!displayEnabled()) return;

    // The LY=LYC flag compares a new LY one M-cycle after LY changes
    if (lyCompareDelay > 0)
    {
        lyCompareDelay = cycles.cycles >= lyCompareDelay ? 0 : lyCompareDelay - cycles.cycles;
        if (lyCompareDelay == 0) compareLYToLYC();
    }

    // VBlank's interrupts, and line 0's mode 2 STAT source, rise a few dots
    // into their line
    if (lateInterruptDelay > 0)
    {
        lateInterruptDelay = cycles.cycles >= lateInterruptDelay ? 0 : lateInterruptDelay - cycles.cycles;
        if (lateInterruptDelay == 0) raiseLateInterrupts();
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
        // No OAM scan on this first line: go straight to drawing. The line is
        // 4 dots short, and drawing starts LCD_ENABLE_DELAY dots in.
        checkWindowYTrigger();
        startMode3(ppu::CLOCKS_PER_SCANLINE - ppu::LCD_ENABLE_FIRST_LINE_SHORTFALL, ppu::LCD_ENABLE_DELAY);
        compareLYToLYC();
        refreshStatSignal(true);
    }

    cycleCounter += elapsed;

    // Outside mode 3, pixels still on their way to the LCD keep moving
    if (currentMode != VideoMode::AccessVRAM) advanceLCD(elapsed);

    switch (currentMode)
    {
        case VideoMode::AccessOAM:
        {
            if (cycleCounter >= ppu::CLOCKS_PER_SCANLINE_OAM)
            {
                cycleCounter -= ppu::CLOCKS_PER_SCANLINE_OAM;
                startMode3(ppu::CLOCKS_PER_SCANLINE, ppu::CLOCKS_PER_SCANLINE_OAM);
                refreshStatSignal(true);
                // Draw the dots of mode 3 already in this tick
                runMode3();
            }
            break;
        }

        case VideoMode::AccessVRAM:
        {
            runMode3();
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

                    // Render the finished frame before resetting anything
                    draw();
                    buffer.reset();

                    // Window state resets for the next frame
                    windowLine = 0;
                    windowYTriggered = false;
                    windowFromLineStart = false;

                    // The VBlank interrupts come a little later
                    lateInterruptDelay = ppu::LATE_VBLANK_DOTS;
                    startLYCompare();
                    refreshStatSignal(true);

                    // On the CGB, line 144 raises the mode 2 STAT source briefly as
                    // it starts, a cycle before the VBlank interrupt (mooneye's
                    // vblank_stat_intr-C; on the DMG it comes with VBlank)
                    if (emulator.cgbHardware)
                    {
                        line144OAMPulse = true;
                        refreshStatSignal(true);
                        line144OAMPulse = false;
                        refreshStatSignal(true);
                    }
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

                if (vblankLine >= static_cast<int32_t>(ppu::VBLANK_LINES))
                {
                    // End of VBlank: restart rendering at line 0
                    ly153EarlyZero = false;
                    vblankLine = 0;
                    line.set(0);
                    setMode(VideoMode::AccessOAM);
                    lateInterruptDelay = ppu::LATE_VBLANK_DOTS;
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

    // On the CGB, clearing the window enable bit also clears the Y condition
    // https://gbdev.io/pandocs/Window.html#window-rendering-criteria
    if (emulator.cgbHardware && windowEnabled() && !checkBit(value, ppu::LCDC_WINDOW_ENABLE_BIT)) windowYTriggered = false;

    controlByte = value;

    // LCDC bits 0 and 1 reach the LCD a dot later on the DMG, except that
    // clearing bit 1 before the LCD has drawn the line's first pixel takes
    // effect at once (Mealybug Tearoom's captures; SameBoy does the same)
    if (displayEnabled() && wasEnabled && !emulator.cgbHardware)
    {
        pendingOutputControl = value;
        outputControlDots = ppu::LCD_CONTROL_OUTPUT_DELAY;
        const bool firstPixelNext = currentMode == VideoMode::AccessVRAM && pixelX == lcdPipelineSize;
        if (firstPixelNext && !checkBit(value, ppu::LCDC_SPRITES_ENABLE_BIT))
            outputControl = setBitTo(outputControl, ppu::LCDC_SPRITES_ENABLE_BIT, false);
    }
    else
    {
        outputControl = value;
        outputControlDots = 0;
    }

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
        lateInterruptDelay = 0;
        lyCompareDelay = 0;
        lcdPipelineSize = 0;
        if (paletteGlitchDots > 0) finishPaletteWrite();
        // STAT reads mode 0 while the PPU is off. The LY=LYC flag and the STAT
        // line keep their values: nothing updates them until it is back on.
        showModeInSTAT();
        buffer.reset();
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
    if (emulator.cgbHardware || !displayEnabled() || screenEnableDelay > 0) return -1;
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


void PPU::registerVBlankCallback(const std::function<void(const FrameBuffer&)> &vblankCallback)
{
    this->vblankCallback = vblankCallback;
}

void PPU::draw()
{
    // The Super Game Boy reads VRAM transfers from the picture, and once a
    // game has sent a border it shows the picture inside it
    if (emulator.sgbMode)
    {
        emulator.sgb.frameFinished(buffer);
        if (emulator.sgb.hasBorder())
        {
            vblankCallback(emulator.sgb.compose(buffer));
            return;
        }
    }
    vblankCallback(buffer);
}
