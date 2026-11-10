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

void PPU::setMode(VideoMode mode)
{
    currentMode = mode;
    const uint8_t otherBits = lcdStatus.getValue() & static_cast<uint8_t>(~ppu::STAT_MODE_MASK);
    lcdStatus.set(otherBits | static_cast<uint8_t>(mode));
}

void PPU::tick(Cycles cycles)
{
    if (!displayEnabled()) return;

    if (screenEnableDelay > 0)
    {
        if (cycles.cycles < screenEnableDelay)
        {
            screenEnableDelay -= cycles.cycles;
            return;
        }
        screenEnableDelay = 0;
        cycleCounter = 0;
        setMode(VideoMode::AccessOAM);
    }

    cycleCounter += cycles.cycles;

    switch (currentMode)
    {
        case VideoMode::AccessOAM:
        {
            if (cycleCounter >= ppu::CLOCKS_PER_SCANLINE_OAM)
            {
                cycleCounter = cycleCounter % ppu::CLOCKS_PER_SCANLINE_OAM;
                setMode(VideoMode::AccessVRAM);
            }
            break;
        }

        case VideoMode::AccessVRAM:
        {
            if (cycleCounter >= ppu::CLOCKS_PER_SCANLINE_VRAM)
            {
                cycleCounter = cycleCounter % ppu::CLOCKS_PER_SCANLINE_VRAM;
                setMode(VideoMode::HBlank);
                emulator.mmu.hblankDMA();

                bool hblankInterrupt = checkBit(lcdStatus.getValue(), ppu::STAT_HBLANK_INTERRUPT_BIT);

                if (hblankInterrupt)
                {
                    emulator.cpu.interruptFlag.setBitTo(cpu::INTERRUPT_LCD_STATUS_BIT, true);
                }

                bool lyCoincidenceInterrupt = checkBit(lcdStatus.getValue(), ppu::STAT_LYC_INTERRUPT_BIT);
                bool lyCoincidence = lyCompare.getValue() == line.getValue();

                if (lyCoincidenceInterrupt && lyCoincidence)
                {
                    emulator.cpu.interruptFlag.setBitTo(cpu::INTERRUPT_LCD_STATUS_BIT, true);
                }

                lcdStatus.setBitTo(ppu::STAT_COINCIDENCE_BIT, lyCoincidence);
            }
            break;
        }

        case VideoMode::HBlank:
        {
            if (cycleCounter >= ppu::CLOCKS_PER_HBLANK)
            {
                writeScanline(line.getValue());
                line.increment();

                cycleCounter = cycleCounter % ppu::CLOCKS_PER_HBLANK;

                // Line 145 (index 144) is the first line of VBLANK
                if (line == ppu::SCANLINES_PER_FRAME)
                {
                    setMode(VideoMode::VBlank);
                    emulator.cpu.interruptFlag.setBitTo(cpu::INTERRUPT_VBLANK_BIT, true);
                }

                else
                {
                    setMode(VideoMode::AccessOAM);
                }
            }
            break;
        }

        case VideoMode::VBlank:
        {
            if (cycleCounter >= ppu::CLOCKS_PER_SCANLINE)
            {
                line.increment();
                cycleCounter = cycleCounter % ppu::CLOCKS_PER_SCANLINE;

                // Line 155 (index 154) is the last line
                if (line == ppu::SCANLINES_PER_FRAME_WITH_VBLANK)
                {
                    writeSprites();
                    draw();
                    buffer.reset();
                    bgAttributePriority.fill(0);
                    line.reset();
                    setMode(VideoMode::AccessOAM);
                }
            }
        }
            break;
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
        lcdStatus.setBitTo(ppu::STAT_COINCIDENCE_BIT, lyCompare.getValue() == 0);
        buffer.reset();
    }
    else if (!wasEnabled && nowEnabled)
    {
        screenEnableDelay = ppu::LCD_ENABLE_DELAY;
        cycleCounter = 0;
        setMode(VideoMode::HBlank);
        line.set(0);
        lcdStatus.setBitTo(ppu::STAT_COINCIDENCE_BIT, lyCompare.getValue() == 0);
    }
}

bool PPU::displayEnabled() const { return checkBit(controlByte, ppu::LCDC_DISPLAY_ENABLE_BIT); }
bool PPU::cpuCanAccessVRAM() const
{
    const uint8_t mode = lcdStatus.getValue() & ppu::STAT_MODE_MASK;
    return !displayEnabled() || mode != static_cast<uint8_t>(VideoMode::AccessVRAM);
}

bool PPU::cpuCanAccessOAM() const
{
    if (!displayEnabled()) return true;
    const uint8_t mode = lcdStatus.getValue() & ppu::STAT_MODE_MASK;
    return mode != static_cast<uint8_t>(VideoMode::AccessOAM) &&
           mode != static_cast<uint8_t>(VideoMode::AccessVRAM);
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

    if (windowEnabled())
    {
        drawWindowLine(currentLine);
    }
}

void PPU::writeSprites()
{
    if (!spritesEnabled()) return;
    std::array<uint8_t, ppu::MAX_SPRITES> order{};
    for (uint8_t i = 0; i < order.size(); ++i) order[i] = i;
    const bool oamPriority = emulator.cgbMode && objectPriorityMode == 0;
    std::sort(order.begin(), order.end(), [this, oamPriority](uint8_t a, uint8_t b)
    {
        if (oamPriority) return a > b; // Draw lower-priority entries first.
        const uint8_t xA = emulator.mmu.read(Address(mmu::OAM_ADDRESS_START + a * ppu::SPRITE_BYTES + ppu::OAM_SPRITE_X_BYTE), true);
        const uint8_t xB = emulator.mmu.read(Address(mmu::OAM_ADDRESS_START + b * ppu::SPRITE_BYTES + ppu::OAM_SPRITE_X_BYTE), true);
        return xA == xB ? a > b : xA > xB;
    });
    for (uint8_t sprite : order) drawSprite(sprite);
}

void PPU::drawBGLine(uint32_t currentLine)
{
    // Note: tile set 0 uses signed numbering
    // to share half of the tiles with tile set 1
    bool useTileSetZero = bgWindowTileData();
    bool useTileMapZero = !usesBackgroundTileMap();

    ColorPalette palette = loadPalette(bgPalette);

    Address tileSetAddress = useTileSetZero ? ppu::TILE_SET_ZERO_ADDRESS : ppu::TILE_SET_ONE_ADDRESS;
    Address tileMapAddress = useTileMapZero ? ppu::TILE_MAP_ZERO_ADDRESS : ppu::TILE_MAP_ONE_ADDRESS;

    // The pixel row we're drawing on the screen
    // is constant since we're only drawing a single line
    uint32_t screenY = currentLine;

    for (uint32_t screenX = 0; screenX < common::GAMEBOY_WIDTH; screenX++)
    {
        // Get the position of the pixel in the framebuffer
        uint32_t scrolledX = screenX + scrollX.getValue();
        uint32_t scrolledY = screenY + scrollY.getValue();

        // Get the index of the pixel in the full background map
        uint32_t bgMapX = scrolledX % common::BACKGROUND_MAP_SIZE;
        uint32_t bgMapY = scrolledY % common::BACKGROUND_MAP_SIZE;

        // Find which tile of the background map the pixel is in,
        // and the index of that tile in the array of tiles
        uint32_t tileX = bgMapX / ppu::TILE_WIDTH_PX;
        uint32_t tileY = bgMapY / ppu::TILE_HEIGHT_PX;

        // Find which specific (x, y) coordinate inside
        // the tile we will render
        uint32_t tilePixelX = bgMapX % ppu::TILE_WIDTH_PX;
        uint32_t tilePixelY = bgMapY % ppu::TILE_HEIGHT_PX;

        // Find the memory address of the tile ID from the tile map
        uint32_t tileIndex = tileY * ppu::TILES_PER_LINE + tileX;
        Address tileIDAddress = tileMapAddress + tileIndex;

        // Grab the ID of the tile we'll get data from
        uint8_t tileID = emulator.mmu.read(tileIDAddress, true);
        uint8_t tileAttributes = emulator.cgbMode
            ? readVRAMBank(tileIDAddress.getValue() - mmu::VRAM_ADDRESS_START, 1) : 0; // Attributes live in VRAM bank 1

        // Calculate the offset from the start of the tile data
        // memory where the data from in the tile map
        uint32_t tileDataMemoryOffset = useTileSetZero
            ? tileID * ppu::TILE_BYTES
            : (static_cast<int8_t>(tileID) + 128) * ppu::TILE_BYTES; // Signed IDs: tile 0 is in the middle of the set

        // Calculate the extra offset to the data for the line
        // of pixles we are rendering from (2 bytes per line of pixels * y lines)
        uint32_t row = checkBit(tileAttributes, ppu::ATTRIBUTE_Y_FLIP_BIT) ? (ppu::TILE_HEIGHT_PX - 1) - tilePixelY : tilePixelY;
        uint32_t column = checkBit(tileAttributes, ppu::ATTRIBUTE_X_FLIP_BIT) ? (ppu::TILE_WIDTH_PX - 1) - tilePixelX : tilePixelX;
        uint32_t tileDataOffset = tileSetAddress.getValue() - mmu::VRAM_ADDRESS_START + tileDataMemoryOffset + row * ppu::TILE_BYTES_PER_ROW;
        const uint8_t tileBank = getBitValue(tileAttributes, ppu::ATTRIBUTE_VRAM_BANK_BIT);
        uint8_t pixelsOne = readVRAMBank(tileDataOffset, tileBank);
        uint8_t pixelsTwo = readVRAMBank(tileDataOffset + 1, tileBank);
        GBColor pixelColor = getPixelFromLine(pixelsOne, pixelsTwo, column);
        const uint8_t shade = static_cast<uint8_t>(pixelColor);
        bgAttributePriority[screenY * common::GAMEBOY_WIDTH + screenX] = getBitValue(tileAttributes, ppu::ATTRIBUTE_PRIORITY_BIT);
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
    // Note: tile set 2 uses signed numbering
    // to share half of the tiles with tile set 1
    bool useTileSetZero = bgWindowTileData();
    bool useTileMapZero = !usesWindowTileMap();

    ColorPalette palette = loadPalette(bgPalette);

    Address tileSetAddress = useTileSetZero ? ppu::TILE_SET_ZERO_ADDRESS : ppu::TILE_SET_ONE_ADDRESS;
    Address tileMapAddress = useTileMapZero ? ppu::TILE_MAP_ZERO_ADDRESS : ppu::TILE_MAP_ONE_ADDRESS;

    // The pixel row we're drawing on the screen
    // is constant since we're only drawing a single line
    uint32_t screenY = currentLine;
    uint32_t scrolledY = screenY - windowY.getValue();
    if (scrolledY >= common::GAMEBOY_HEIGHT) return;

    for (uint32_t screenX = 0; screenX < common::GAMEBOY_WIDTH; screenX++)
    {
        // Get the position of the pixel in the framebuffer
        uint32_t scrolledX = screenX + windowX.getValue() - ppu::WINDOW_X_OFFSET;

        // Find which tile of the background map the pixel is in,
        // and the index of that tile in the array of tiles
        uint32_t tileX = scrolledX / ppu::TILE_WIDTH_PX;
        uint32_t tileY = scrolledY / ppu::TILE_HEIGHT_PX;

        // Find which specific (x, y) coordinate inside
        // the tile we will render
        uint32_t tilePixelX = scrolledX % ppu::TILE_WIDTH_PX;
        uint32_t tilePixelY = scrolledY % ppu::TILE_HEIGHT_PX;

        // Find the memory address of the tile ID from the tile map
        uint32_t tileIndex = tileY * ppu::TILES_PER_LINE + tileX;
        Address tileIDAddress = tileMapAddress + tileIndex;

        // Grab the ID of the tile we'll get data from
        uint8_t tileID = emulator.mmu.read(tileIDAddress, true);
        uint8_t tileAttributes = emulator.cgbMode
            ? readVRAMBank(tileIDAddress.getValue() - mmu::VRAM_ADDRESS_START, 1) : 0; // Attributes live in VRAM bank 1

        // Calculate the offset from the start of the tile data
        // memory where the data from in the tile map
        uint32_t tileDataMemoryOffset = useTileSetZero
            ? tileID * ppu::TILE_BYTES
            : (static_cast<int8_t>(tileID) + 128) * ppu::TILE_BYTES; // Signed IDs: tile 0 is in the middle of the set

        // Calculate the extra offset to the data for the line
        // of pixles we are rendering from (2 bytes per line of pixels * y lines)
        uint32_t row = checkBit(tileAttributes, ppu::ATTRIBUTE_Y_FLIP_BIT) ? (ppu::TILE_HEIGHT_PX - 1) - tilePixelY : tilePixelY;
        uint32_t column = checkBit(tileAttributes, ppu::ATTRIBUTE_X_FLIP_BIT) ? (ppu::TILE_WIDTH_PX - 1) - tilePixelX : tilePixelX;
        uint32_t tileDataOffset = tileSetAddress.getValue() - mmu::VRAM_ADDRESS_START + tileDataMemoryOffset + row * ppu::TILE_BYTES_PER_ROW;
        const uint8_t tileBank = getBitValue(tileAttributes, ppu::ATTRIBUTE_VRAM_BANK_BIT);
        uint8_t pixelsOne = readVRAMBank(tileDataOffset, tileBank);
        uint8_t pixelsTwo = readVRAMBank(tileDataOffset + 1, tileBank);
        GBColor pixelColor = getPixelFromLine(pixelsOne, pixelsTwo, column);
        const uint8_t shade = static_cast<uint8_t>(pixelColor);
        bgAttributePriority[screenY * common::GAMEBOY_WIDTH + screenX] = getBitValue(tileAttributes, ppu::ATTRIBUTE_PRIORITY_BIT);
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

void PPU::drawSprite(const uint32_t sprite)
{
    // Each sprite is represented by 4 bytes,
    // or 8 bytes in 8x16 mode
    Address offsetInOAM = sprite * ppu::SPRITE_BYTES;

    Address oamStart = mmu::OAM_ADDRESS_START + offsetInOAM.getValue();
    uint8_t spriteY = emulator.mmu.read(oamStart + ppu::OAM_SPRITE_Y_BYTE, true);
    uint8_t spriteX = emulator.mmu.read(oamStart + ppu::OAM_SPRITE_X_BYTE, true);

    // If the sprite would be drawn outside the screen,
    // don't draw it at all
    if (spriteY == 0 || spriteY >= common::GAMEBOY_HEIGHT + ppu::SPRITE_Y_OFFSET) return;
    if (spriteX == 0 || spriteX >= common::GAMEBOY_WIDTH + ppu::SPRITE_X_OFFSET) return;

    uint32_t spriteSizeMultiplier = usesLargeSprites() ? 2 : 1;

    // Sprites are always taken from the first tile set
    Address tileSetLocation = ppu::TILE_SET_ZERO_ADDRESS;

    uint8_t pattern = emulator.mmu.read(oamStart + ppu::OAM_SPRITE_TILE_BYTE, true);
    uint8_t spriteAttributes = emulator.mmu.read(oamStart + ppu::OAM_SPRITE_ATTRIBUTE_BYTE, true);

    // Bits 0-3 are used only for CGB
    bool usePaletteOne = checkBit(spriteAttributes, ppu::ATTRIBUTE_DMG_PALETTE_BIT);
    bool flipX = checkBit(spriteAttributes, ppu::ATTRIBUTE_X_FLIP_BIT);
    bool flipY = checkBit(spriteAttributes, ppu::ATTRIBUTE_Y_FLIP_BIT);
    bool objectBehindBG = checkBit(spriteAttributes, ppu::ATTRIBUTE_PRIORITY_BIT);

    ColorPalette palette = usePaletteOne
        ? loadPalette(spritePalette1)
        : loadPalette(spritePalette0);

    uint32_t tileOffset = pattern * ppu::TILE_BYTES;
    Address patternAddress = tileSetLocation + tileOffset;

    const uint8_t previousBank = vramBank;
    if (emulator.cgbMode) vramBank = getBitValue(spriteAttributes, ppu::ATTRIBUTE_VRAM_BANK_BIT);
    Tile tile(patternAddress, emulator.mmu, spriteSizeMultiplier);
    vramBank = previousBank;
    int32_t startY = spriteY - ppu::SPRITE_Y_OFFSET;
    int32_t startX = spriteX - ppu::SPRITE_X_OFFSET;

    for (uint32_t y = 0; y < ppu::TILE_HEIGHT_PX * spriteSizeMultiplier; y++)
    {
        for (uint32_t x = 0; x < ppu::TILE_WIDTH_PX; x++)
        {
            uint32_t maybeFlippedY = !flipY ? y : (ppu::TILE_HEIGHT_PX * spriteSizeMultiplier) - y - 1;
            uint32_t maybeFlippedX = !flipX ? x : ppu::TILE_WIDTH_PX - x - 1;

            GBColor gbColor = tile.getPixel(maybeFlippedX, maybeFlippedY);

            // Color 0 is transparent
            if (gbColor == GBColor::Color0) continue;

            int32_t screenX = startX + x;
            int32_t screenY = startY + y;

            if (!isOnScreen(screenX, screenY)) continue;

            Color existingPixel = buffer.getPixel(screenX, screenY);

            const uint8_t shade = static_cast<uint8_t>(gbColor);
            const bool bgNonzero = existingPixel != Color::White;
            if (emulator.cgbMode)
            {
                const bool bgPriority = bgAttributePriority[screenY * common::GAMEBOY_WIDTH + screenX] != 0;
                if ((objectBehindBG || bgPriority) && bgNonzero && bgEnabled()) continue;
                buffer.setPixelRGB(screenX, screenY,
                    objColorPalettes.getColor(spriteAttributes & ppu::ATTRIBUTE_CGB_PALETTE_MASK, shade), shade);
            }
            else
            {
                if (objectBehindBG && bgNonzero) continue;
                if (emulator.sgbMode)
                {
                    const uint8_t mappedShade = static_cast<uint8_t>(getColorFromPalette(gbColor, palette));
                    const uint8_t paletteIndex = emulator.sgb.getAttribute(screenX, screenY);
                    buffer.setPixelRGB(screenX, screenY, emulator.sgb.getColor(paletteIndex, mappedShade, screenX, screenY), mappedShade);
                }
                else buffer.setPixel(screenX, screenY, getColorFromPalette(gbColor, palette));
            }
        }
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
    //return x < common::GAMEBOY_WIDTH;
    return (x >= 0) && (x < common::GAMEBOY_WIDTH);
}

bool PPU::isOnScreenY(int32_t y)
{
    //return y < common::GAMEBOY_HEIGHT;
    return (y >= 0) && (y < common::GAMEBOY_HEIGHT);
}

bool PPU::isOnScreen(int32_t x, int32_t y)
{
    return isOnScreenX(x) && isOnScreenY(y);
}

ColorPalette PPU::loadPalette(ByteRegister &paletteRegister)
{
    // Each palette register packs four 2-bit shades:
    // bits 1-0 for color 0, bits 3-2 for color 1,
    // bits 5-4 for color 2, and bits 7-6 for color 3
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
        default: std::runtime_error("Invalid color");
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
