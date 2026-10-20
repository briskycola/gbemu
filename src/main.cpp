#include "emulator.hpp"
#include "framebuffer.hpp"
#include "input.hpp"
#include "file.hpp"
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <iostream>
#include <fstream>
#include <iterator>
#include <SDL2/SDL.h>

// The size of the actual pixels on the SDL window
static constexpr uint32_t pixelSize = 6;
static constexpr uint32_t width = GAMEBOY_WIDTH * pixelSize;
static constexpr uint32_t height = GAMEBOY_HEIGHT * pixelSize;

// SDL stuff
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *gbScreenTexture;
static SDL_AudioDeviceID audioDevice;

static std::unique_ptr<Emulator> emulator;
static bool shouldExit = false;

bool tryToGetGBButton(int32_t keyCode, GBButton &button)
{
    switch (keyCode)
    {
        case SDLK_w:         button = GBButton::Up;     return true;
        case SDLK_s:         button = GBButton::Down;   return true;
        case SDLK_a:         button = GBButton::Left;   return true;
        case SDLK_d:         button = GBButton::Right;  return true;
        case SDLK_p:         button = GBButton::A;      return true;
        case SDLK_o:         button = GBButton::B;      return true;
        case SDLK_BACKSPACE: button = GBButton::Select; return true;
        case SDLK_RETURN:    button = GBButton::Start;  return true;
        default: return false;
    }
}

uint32_t getRealColor(Color color)
{
    uint32_t r, g, b;

    switch (color)
    {
        case Color::White: r = g = b = 255; break;
        case Color::LightGray: r = g = b = 170; break;
        case Color::DarkGray: r = g = b = 85; break;
        case Color::Black: r = g = b = 0; break;
    }
    return (r << 16) | (g << 8) | (b << 0);
    //return 0xFF000000u | (r << 16) | (g << 8) | b;
}

void setPixel(uint32_t *pixels, uint32_t x, uint32_t y, uint32_t pixelARGB)
{
    pixels[width * y + x] = pixelARGB;
}

void setLargePixel(uint32_t *pixels, uint x, uint y, uint32_t pixelARGB)
{
    for (uint32_t width = 0; width < pixelSize; width++)
    {
        for (uint32_t height = 0; height < pixelSize; height++)
        {
            setPixel(pixels, x * pixelSize + width, y * pixelSize + height, pixelARGB);
        }
    }
}

void setPixels(uint32_t *pixels, const FrameBuffer& buffer)
{
    for (uint32_t y = 0; y < GAMEBOY_HEIGHT; y++)
    {
        for (uint32_t x = 0; x < GAMEBOY_WIDTH; x++)
        {
            Color color = buffer.getPixel(x, y);
            uint32_t pixelARGB = getRealColor(color);
            setLargePixel(pixels, x, y, pixelARGB);
        }
    }
}

//static inline void setPixel(uint32_t* pixels, int pitchBytes, uint32_t x, uint32_t y, uint32_t argb)
//{
//    uint32_t stride = (uint32_t)(pitchBytes / 4);
//    pixels[stride * y + x] = argb;
//}
//
//static inline void setLargePixel(uint32_t* pixels, int pitchBytes, uint32_t x, uint32_t y, uint32_t argb)
//{
//    for (uint32_t dx = 0; dx < pixelSize; dx++)
//        for (uint32_t dy = 0; dy < pixelSize; dy++)
//            setPixel(pixels, pitchBytes, x * pixelSize + dx, y * pixelSize + dy, argb);
//}
//
//void setPixels(uint32_t* pixels, int pitchBytes, const FrameBuffer& buffer)
//{
//    for (uint32_t y = 0; y < GAMEBOY_HEIGHT; y++)
//        for (uint32_t x = 0; x < GAMEBOY_WIDTH; x++)
//            setLargePixel(pixels, pitchBytes, x, y, getRealColor(buffer.getPixel(x, y)));
//}

std::string getSaveFilename(const std::string &filename)
{
    return filename + ".battery";
}

bool fileExists(const std::string &filename)
{
    std::ifstream file(filename.c_str());
    return file.good();
}

void saveState(const std::string &filename)
{
    std::vector<uint8_t> cartridgeRAM = emulator->getCartridgeRAM();

    // Don't save if the cartridge
    // has no RAM
    if (cartridgeRAM.size() == 0) return;

    std::string saveFile = getSaveFilename(filename);
    std::ofstream outputFile(saveFile);
    std::copy(cartridgeRAM.begin(), cartridgeRAM.end(), std::ostreambuf_iterator<char>(outputFile));
}

std::vector<uint8_t> loadState(const std::string &filename)
{
    std::string saveFile = getSaveFilename(filename);
    if (!fileExists(saveFile))
    {
        return {};
    }

    else
    {
        std::vector<uint8_t> saveData = readFile(saveFile);
        return saveData;
    }
}

void processSDLEvents()
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_KEYDOWN:
            {
                if (event.key.repeat == true) break;
                GBButton button;

                if (tryToGetGBButton(event.key.keysym.sym, button))
                {
                    emulator->buttonPressed(button);
                }
                break;
            }

            case SDL_KEYUP:
            {
                if (event.key.repeat == true) break;
                GBButton button;

                if (tryToGetGBButton(event.key.keysym.sym, button))
                {
                    emulator->buttonReleased(button);
                }
                break;
            }

            case SDL_WINDOWEVENT:
            {
                if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                {
                    shouldExit = true;
                }
                break;
            }

            case SDL_QUIT:
            {
                shouldExit = true;
                break;
            }

            default: break;
        }
    }
}

void draw(const FrameBuffer &buffer)
{
    processSDLEvents();

    std::vector<int16_t> audioSamples = emulator->takeAudioSamples();
    if (audioDevice != 0)
    {
        if (!audioSamples.empty())
        {
            const uint32_t queuedBytes = SDL_GetQueuedAudioSize(audioDevice);
            if (queuedBytes > 192000) SDL_ClearQueuedAudio(audioDevice);
            SDL_QueueAudio(audioDevice, audioSamples.data(),
                static_cast<uint32_t>(audioSamples.size() * sizeof(int16_t)));
        }
    }

    SDL_RenderClear(renderer);

    void *pixelsPointer = nullptr;
    int32_t pitch = 0;

    SDL_LockTexture(gbScreenTexture, nullptr, &pixelsPointer, &pitch);

    uint32_t *pixels = static_cast<uint32_t*>(pixelsPointer);
    setPixels(pixels, buffer);
    //setPixels((uint32_t*)pixelsPointer, pitch, buffer);

    SDL_UnlockTexture(gbScreenTexture);

    SDL_RenderCopy(renderer, gbScreenTexture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

bool isClosed()
{
    return shouldExit;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <rom-file>\n";
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "Failed to initalize SDL\n";
        return 2;
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0)
    {
        SDL_AudioSpec audioSpec{};
        audioSpec.freq = 48000;
        audioSpec.format = AUDIO_S16SYS;
        audioSpec.channels = 2;
        audioSpec.samples = 1024;
        audioDevice = SDL_OpenAudioDevice(nullptr, 0, &audioSpec, nullptr, 0);
        if (audioDevice != 0) SDL_PauseAudioDevice(audioDevice, 0);
        else std::cerr << "Audio output unavailable: " << SDL_GetError() << "\n";
    }

    //SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow("gbemu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL);
    
    if (window == nullptr)
    {
        std::cerr << "Failed to create SDL window\n";
        return 3;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    //if (renderer == nullptr)
    //{
    //    std::cerr << "Failed to create SDL renderer\n";
    //    return 4;
    //}

    // TEMP: using vsync
    //SDL_SetRenderVSync(renderer, 1);

    gbScreenTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    //if (gbScreenTexture == nullptr)
    //{
    //    std::cerr << "Failed to create SDL texture\n";
    //    return 5;
    //}

    std::vector<uint8_t> romData = readFile(argv[1]);
    std::vector<uint8_t> saveData = loadState(argv[1]);

    emulator = std::make_unique<Emulator>(romData, saveData);
    emulator->run(&isClosed, &draw);

    saveState(argv[1]);

    SDL_DestroyTexture(gbScreenTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    if (audioDevice != 0) SDL_CloseAudioDevice(audioDevice);
    SDL_Quit();

    return 0;
}
