#include "ppu.hpp"
#include "color.hpp"
#include "emulator.hpp"
#include "common.hpp"
#include "tile.hpp"
#include <stdexcept>

PPU::PPU(Emulator &emulator) :
    emulator(emulator),
    buffer(GAMEBOY_WIDTH, GAMEBOY_HEIGHT),
    backgroundMap(BACKGROUND_MAP_SIZE, BACKGROUND_MAP_SIZE)
{
    videoRAM = std::vector<uint8_t>(VRAM_SIZE_BYTES);
}

uint8_t PPU::read(const Address &address)
{
    return videoRAM.at(address.getValue());
}

void PPU::write(const Address &address, uint8_t value)
{
    videoRAM.at(address.getValue()) = value;
}

void PPU::tick(Cycles cycles)
{
    cycleCounter += cycles.cycles;

    switch (currentMode)
    {
        case VideoMode::ACCESS_OAM:
        {
            if (cycleCounter >= CLOCKS_PER_SCANLINE_OAM)
            {
                cycleCounter = cycleCounter % CLOCKS_PER_SCANLINE_OAM;
                lcdStatus.setBitTo(1, true);
                lcdStatus.setBitTo(0, true);
                currentMode = VideoMode::ACCESS_VRAM;
            }
            break;
        }

        case VideoMode::ACCESS_VRAM:
        {
            if (cycleCounter >= CLOCKS_PER_SCANLINE_VRAM)
            {
                cycleCounter = cycleCounter % CLOCKS_PER_SCANLINE_VRAM;
                currentMode = VideoMode::HBLANK;

                bool hblankInterrupt = checkBit(lcdStatus.getValue(), 3);

                if (hblankInterrupt)
                {
                    emulator.cpu.interruptFlag.setBitTo(1, true);
                }

                bool lyCoincidenceInterrupt = checkBit(lcdStatus.getValue(), 6);
                bool lyCoincidence = lyCompare.getValue() == line.getValue();

                if (lyCoincidenceInterrupt && lyCoincidence)
                {
                    emulator.cpu.interruptFlag.setBitTo(1, true);
                }

                lcdStatus.setBitTo(2, lyCoincidence);
                lcdStatus.setBitTo(1, false);
                lcdStatus.setBitTo(0, false);
            }
            break;
        }

        case VideoMode::HBLANK:
        {
            if (cycleCounter >= CLOCKS_PER_HBLANK)
            {
                writeScanline(line.getValue());
                line.increment();

                cycleCounter = cycleCounter % CLOCKS_PER_HBLANK;

                // Line 145 (index 144) is the first line of VBLANK
                if (line == 144)
                {
                    currentMode = VideoMode::VBLANK;
                    lcdStatus.setBitTo(1, false);
                    lcdStatus.setBitTo(0, true);
                    emulator.cpu.interruptFlag.setBitTo(0, true);
                }

                else
                {
                    lcdStatus.setBitTo(1, true);
                    lcdStatus.setBitTo(0, false);
                    currentMode = VideoMode::ACCESS_OAM;
                }
            }
            break;
        }

        case VideoMode::VBLANK:
        {
            if (cycleCounter >= CLOCKS_PER_SCANLINE)
            {
                line.increment();
                cycleCounter = cycleCounter % CLOCKS_PER_SCANLINE;

                // Line 155 (index 154) is the last line
                if (line == 154)
                {
                    writeSprites();
                    draw();
                    buffer.reset();
                    line.reset();
                    currentMode = VideoMode::ACCESS_OAM;
                    lcdStatus.setBitTo(1, true);
                    lcdStatus.setBitTo(0, false);
                }
            }
        }
            break;
    }
}

bool PPU::displayEnabled() const { return checkBit(controlByte, 7); }
bool PPU::windowTileMap() const { return checkBit(controlByte, 6); }
bool PPU::windowEnabled() const { return checkBit(controlByte, 5); }
bool PPU::bgWindowTileData() const { return checkBit(controlByte, 4); }
bool PPU::bgTileMapDisplay() const { return checkBit(controlByte, 3); }
bool PPU::spriteSize() const { return checkBit(controlByte, 2); }
bool PPU::spritesEnabled() const { return checkBit(controlByte, 1); }
bool PPU::bgEnabled() const { return checkBit(controlByte, 0); }

void PPU::writeScanline(uint8_t currentLine)
{
    if (!displayEnabled()) return;

    if (bgEnabled())
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

    for (uint32_t sprite = 0; sprite < 40; sprite++)
    {
        drawSprite(sprite);
    }
}

void PPU::drawBGLine(uint32_t currentLine)
{
    // Note: tile set 0 uses signed numbering
    // to share half of the tiles with tile set 1
    bool useTileSetZero = bgWindowTileData();
    bool useTileMapZero = !bgTileMapDisplay();

    colorPalette palette = loadPalette(bgPalette);

    Address tileSetAddress = useTileSetZero ? TILE_SET_ZERO_ADDRESS : TILE_SET_ONE_ADDRESS;
    Address tileMapAddress = useTileMapZero ? TILE_MAP_ZERO_ADDRESS : TILE_MAP_ONE_ADDRESS;

    // The pixel row we're drawing on the screen
    // is constant since we're only drawing a single line
    uint32_t screenY = currentLine;

    for (uint32_t screenX = 0; screenX < GAMEBOY_WIDTH; screenX++)
    {
        // Get the position of the pixel in the framebuffer
        uint32_t scrolledX = screenX + scrollX.getValue();
        uint32_t scrolledY = screenY + scrollY.getValue();

        // Get the index of the pixel in the full background map
        uint32_t bgMapX = scrolledX % BACKGROUND_MAP_SIZE;
        uint32_t bgMapY = scrolledY % BACKGROUND_MAP_SIZE;

        // Find which tile of the background map the pixel is in,
        // and the index of that tile in the array of tiles
        uint32_t tileX = bgMapX / TILE_WIDTH_PX;
        uint32_t tileY = bgMapY / TILE_HEIGHT_PX;

        // Find which specific (x, y) coordinate inside
        // the tile we will render
        uint32_t tilePixelX = bgMapX % TILE_WIDTH_PX;
        uint32_t tilePixelY = bgMapY % TILE_HEIGHT_PX;

        // Find the memory address of the tile ID from the tile map
        uint32_t tileIndex = tileY * TILES_PER_LINE + tileX;
        Address tileIDAddress = tileMapAddress + tileIndex;

        // Grab the ID of the tile we'll get data from
        uint8_t tileID = emulator.mmu.read(tileIDAddress);

        // Calculate the offset from the start of the tile data
        // memory where the data from in the tile map
        uint32_t tileDataMemoryOffset = useTileSetZero
            ? tileID * TILE_BYTES
            : (static_cast<int8_t>(tileID) + 128) * TILE_BYTES;

        // Calculate the extra offset to the data for the line
        // of pixles we are rendering from (2 bytes per line of pixels * y lines)
        uint32_t tileDataLineOffset = tilePixelY * 2;
        Address tileLineDataStartAddress = tileSetAddress + tileDataMemoryOffset + tileDataLineOffset;

        uint8_t pixelsOne = emulator.mmu.read(tileLineDataStartAddress);
        uint8_t pixelsTwo = emulator.mmu.read(tileLineDataStartAddress + 1);

        GBColor pixelColor = getPixelFromLine(pixelsOne, pixelsTwo, tilePixelX);
        Color screenColor = getColorFromPalette(pixelColor, palette);
        buffer.setPixel(screenX, screenY, screenColor);
    }
}

void PPU::drawWindowLine(uint32_t currentLine)
{
    // Note: tile set 2 uses signed numbering
    // to share half of the tiles with tile set 1
    bool useTileSetZero = bgWindowTileData();
    bool useTileMapZero = !windowTileMap();

    colorPalette palette = loadPalette(bgPalette);

    Address tileSetAddress = useTileSetZero ? TILE_SET_ZERO_ADDRESS : TILE_SET_ONE_ADDRESS;
    Address tileMapAddress = useTileMapZero ? TILE_MAP_ZERO_ADDRESS : TILE_MAP_ONE_ADDRESS;

    // The pixel row we're drawing on the screen
    // is constant since we're only drawing a single line
    uint32_t screenY = currentLine;
    uint32_t scrolledY = screenY - windowY.getValue();
    if (scrolledY >= GAMEBOY_HEIGHT) return;

    for (uint32_t screenX = 0; screenX < GAMEBOY_WIDTH; screenX++)
    {
        // Get the position of the pixel in the framebuffer
        uint32_t scrolledX = screenX + windowX.getValue() - 7;

        // Find which tile of the background map the pixel is in,
        // and the index of that tile in the array of tiles
        uint32_t tileX = scrolledX / TILE_WIDTH_PX;
        uint32_t tileY = scrolledY / TILE_HEIGHT_PX;

        // Find which specific (x, y) coordinate inside
        // the tile we will render
        uint32_t tilePixelX = scrolledX % TILE_WIDTH_PX;
        uint32_t tilePixelY = scrolledY % TILE_HEIGHT_PX;

        // Find the memory address of the tile ID from the tile map
        uint32_t tileIndex = tileY * TILES_PER_LINE + tileX;
        Address tileIDAddress = tileMapAddress + tileIndex;

        // Grab the ID of the tile we'll get data from
        uint8_t tileID = emulator.mmu.read(tileIDAddress);

        // Calculate the offset from the start of the tile data
        // memory where the data from in the tile map
        uint32_t tileDataMemoryOffset = useTileSetZero
            ? tileID * TILE_BYTES
            : (static_cast<int8_t>(tileID) + 128) * TILE_BYTES;

        // Calculate the extra offset to the data for the line
        // of pixles we are rendering from (2 bytes per line of pixels * y lines)
        uint32_t tileDataLineOffset = tilePixelY * 2;
        Address tileLineDataStartAddress = tileSetAddress + tileDataMemoryOffset + tileDataLineOffset;

        uint8_t pixelsOne = emulator.mmu.read(tileLineDataStartAddress);
        uint8_t pixelsTwo = emulator.mmu.read(tileLineDataStartAddress + 1);

        GBColor pixelColor = getPixelFromLine(pixelsOne, pixelsTwo, tilePixelX);
        Color screenColor = getColorFromPalette(pixelColor, palette);
        buffer.setPixel(screenX, screenY, screenColor);
    }
}

void PPU::drawSprite(const uint32_t sprite)
{
    // Each sprite is represented by 4 bytes,
    // or 8 bytes in 8x16 mode
    Address offsetInOAM = sprite * SPRITE_BYTES;

    Address oamStart = 0xFE00 + offsetInOAM.getValue();
    uint8_t spriteY = emulator.mmu.read(oamStart);
    uint8_t spriteX = emulator.mmu.read(oamStart + 1);

    // If the sprite would be drawn outside the screen,
    // don't draw it at all
    if (spriteY == 0 || spriteY >= 160) return;
    if (spriteX == 0 || spriteX >= 168) return;

    uint32_t spriteSizeMultiplier = spriteSize() ? 2 : 1;

    // Sprites are always taken from the first tile set
    Address tileSetLocation = TILE_SET_ZERO_ADDRESS;

    uint8_t pattern = emulator.mmu.read(oamStart + 2);
    uint8_t spriteAttributes = emulator.mmu.read(oamStart + 3);

    // Bits 0-3 are used only for CGB
    bool usePaletteOne = checkBit(spriteAttributes, 4);
    bool flipX = checkBit(spriteAttributes, 5);
    bool flipY = checkBit(spriteAttributes, 6);
    bool objectBehindBG = checkBit(spriteAttributes, 7);

    colorPalette palette = usePaletteOne
        ? loadPalette(spritePalette1)
        : loadPalette(spritePalette0);

    uint32_t tileOffset = pattern * TILE_BYTES;
    Address patternAddress = tileSetLocation + tileOffset;

    Tile tile(patternAddress, emulator.mmu, spriteSizeMultiplier);
    int32_t startY = spriteY - 16;
    int32_t startX = spriteX - 8;

    for (uint32_t y = 0; y < TILE_HEIGHT_PX * spriteSizeMultiplier; y++)
    {
        for (uint32_t x = 0; x < TILE_WIDTH_PX; x++)
        {
            uint32_t maybeFlippedY = !flipY ? y : (TILE_HEIGHT_PX * spriteSizeMultiplier) - y - 1;
            uint32_t maybeFlippedX = !flipX ? x : TILE_WIDTH_PX - x - 1;

            GBColor gbColor = tile.getPixel(maybeFlippedX, maybeFlippedY);

            // Color 0 is transparent
            if (gbColor == GBColor::Color0) continue;

            int32_t screenX = startX + x;
            int32_t screenY = startY + y;

            if (!isOnScreen(screenX, screenY)) continue;

            Color existingPixel = buffer.getPixel(screenX, screenY);

            if (objectBehindBG && existingPixel != Color::White) continue;

            Color screenColor = getColorFromPalette(gbColor, palette);

            buffer.setPixel(screenX, screenY, screenColor);
        }
    }
}

GBColor PPU::getPixelFromLine(uint8_t byteOne, uint8_t byteTwo, uint8_t pixelIndex)
{
    uint8_t color =
        static_cast<uint8_t>((getBitValue(byteTwo, 7 - pixelIndex) << 1) | getBitValue(byteOne, 7 - pixelIndex));
    return getColor(color);
}

bool PPU::isOnScreenX(int32_t x)
{
    //return x < GAMEBOY_WIDTH;
    return (x >= 0) && (x < GAMEBOY_WIDTH);
}

bool PPU::isOnScreenY(int32_t y)
{
    //return y < GAMEBOY_HEIGHT;
    return (y >= 0) && (y < GAMEBOY_HEIGHT);
}

bool PPU::isOnScreen(int32_t x, int32_t y)
{
    return isOnScreenX(x) && isOnScreenY(y);
}

colorPalette PPU::loadPalette(ByteRegister &paletteRegister)
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

Color PPU::getColorFromPalette(GBColor color, const colorPalette &palette) const
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
