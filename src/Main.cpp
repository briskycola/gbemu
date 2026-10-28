#include "Emulator.hpp"
#include "FrameBuffer.hpp"
#include "Input.hpp"
#include "File.hpp"
#include "FileBrowser.hpp"
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <iterator>

// The scale of the Game Boy screen in the initial window
static constexpr uint32_t PIXEL_SIZE = 6;
static constexpr uint32_t WIDTH = GAMEBOY_WIDTH * PIXEL_SIZE;
static constexpr uint32_t HEIGHT = GAMEBOY_HEIGHT * PIXEL_SIZE;

// Smallest ROM that still contains a full cartridge header
static constexpr size_t MINIMUM_ROM_SIZE = 0x150;

// Optional command line arguments
static constexpr int ROM_ARGUMENT = 1;
static constexpr int BOOT_ROM_ARGUMENT = 2;

// Exit codes
enum ExitCode
{
    ExitSDLInitFailed = 2,
    ExitWindowFailed = 3,
    ExitRendererFailed = 4,
    ExitTextureFailed = 5,
};

// Audio output: stereo, signed 16-bit samples. The device pulls
// 1024 samples at a time, and at most 100 ms of audio is queued
// before the queue is flushed to keep latency down.
static constexpr uint8_t AUDIO_CHANNELS = 2;
static constexpr uint16_t AUDIO_DEVICE_BUFFER_SAMPLES = 1024;
static constexpr uint32_t MAX_QUEUED_AUDIO_BYTES = AUDIO_SAMPLE_RATE / 10 * AUDIO_CHANNELS * sizeof(int16_t);

// Texture pixels are ARGB; the Game Boy's colors are fully opaque
static constexpr uint32_t OPAQUE_ALPHA = 0xFF000000;

// GUI sizes and colors
static constexpr float ERROR_BUTTON_WIDTH = 120.0f;
static const ImU32 PLACEHOLDER_TEXT_COLOR = IM_COL32(200, 200, 200, 255);
static const ImVec2 CENTER_PIVOT(0.5f, 0.5f);

// SDL stuff
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *gbScreenTexture;
static SDL_AudioDeviceID audioDevice;

static std::unique_ptr<Emulator> emulator;
static bool shouldExit = false;

// GUI state
static FileBrowser fileBrowser;
static std::string currentROMPath;
static std::vector<uint8_t> cgbBootROM;
static std::string errorMessage;
static bool showErrorPopup = false;
static float menuBarHeight = 0.0f;

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

void showError(const std::string &message)
{
    std::cerr << message << "\n";
    errorMessage = message;
    showErrorPopup = true;
}

void clearScreenTexture()
{
    void *pixelsPointer = nullptr;
    int32_t pitch = 0;

    if (SDL_LockTexture(gbScreenTexture, nullptr, &pixelsPointer, &pitch) != 0) return;
    for (uint32_t y = 0; y < GAMEBOY_HEIGHT; y++)
    {
        uint32_t *row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(pixelsPointer) + y * pitch);
        std::fill(row, row + GAMEBOY_WIDTH, OPAQUE_ALPHA | shadeToRGB(Color::White));
    }
    SDL_UnlockTexture(gbScreenTexture);
}

void closeROM()
{
    if (!emulator) return;

    saveState(currentROMPath);
    emulator.reset();
    currentROMPath.clear();

    if (audioDevice != 0) SDL_ClearQueuedAudio(audioDevice);
    SDL_SetWindowTitle(window, "gbemu");
}

bool openROM(const std::string &path)
{
    std::vector<uint8_t> romData = readFile(path);
    if (romData.size() < MINIMUM_ROM_SIZE)
    {
        showError("Could not load ROM: " + path);
        return false;
    }

    // Flush the current game's save first, in
    // case the same ROM is being reopened
    if (emulator) saveState(currentROMPath);

    std::unique_ptr<Emulator> newEmulator;
    try
    {
        newEmulator = std::make_unique<Emulator>(romData, loadState(path), cgbBootROM);
    }
    catch (const std::exception &e)
    {
        showError("Could not load ROM: " + std::string(e.what()));
        return false;
    }

    closeROM();
    emulator = std::move(newEmulator);
    currentROMPath = path;
    clearScreenTexture();

    std::filesystem::path romPath(path);
    fileBrowser.setDirectory(romPath.parent_path().empty() ? "." : romPath.parent_path());
    std::string title = "gbemu - " + romPath.filename().string();
    SDL_SetWindowTitle(window, title.c_str());
    return true;
}

void processSDLEvents()
{
    SDL_Event event;
    const ImGuiIO &io = ImGui::GetIO();

    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type)
        {
            case SDL_KEYDOWN:
            {
                // Don't let typing into the GUI press Game Boy buttons
                if (event.key.repeat == true || io.WantCaptureKeyboard || !emulator) break;
                GBButton button;

                if (tryToGetGBButton(event.key.keysym.sym, button))
                {
                    emulator->buttonPressed(button);
                }
                break;
            }

            case SDL_KEYUP:
            {
                // Always forward releases so buttons can't get stuck
                if (event.key.repeat == true || !emulator) break;
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

void queueAudio()
{
    std::vector<int16_t> audioSamples = emulator->takeAudioSamples();
    if (audioDevice != 0)
    {
        if (!audioSamples.empty())
        {
            const uint32_t queuedBytes = SDL_GetQueuedAudioSize(audioDevice);
            if (queuedBytes > MAX_QUEUED_AUDIO_BYTES) SDL_ClearQueuedAudio(audioDevice);
            SDL_QueueAudio(audioDevice, audioSamples.data(),
                static_cast<uint32_t>(audioSamples.size() * sizeof(int16_t)));
        }
    }
}

void updateScreenTexture(const FrameBuffer &buffer)
{
    void *pixelsPointer = nullptr;
    int32_t pitch = 0;

    if (SDL_LockTexture(gbScreenTexture, nullptr, &pixelsPointer, &pitch) != 0) return;
    for (uint32_t y = 0; y < GAMEBOY_HEIGHT; y++)
    {
        uint32_t *row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(pixelsPointer) + y * pitch);
        for (uint32_t x = 0; x < GAMEBOY_WIDTH; x++)
        {
            row[x] = OPAQUE_ALPHA | buffer.getPixelRGB(x, y);
        }
    }
    SDL_UnlockTexture(gbScreenTexture);
}

void drawMenuBar()
{
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Open ROM..."))
        {
            fileBrowser.open();
        }

        if (ImGui::MenuItem("Close ROM", nullptr, false, emulator != nullptr))
        {
            closeROM();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Exit"))
        {
            shouldExit = true;
        }
        ImGui::EndMenu();
    }

    menuBarHeight = ImGui::GetWindowSize().y;
    ImGui::EndMainMenuBar();
}

void drawErrorPopup()
{
    if (showErrorPopup)
    {
        ImGui::OpenPopup("Error");
        showErrorPopup = false;
    }

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, CENTER_PIVOT);
    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(errorMessage.c_str());
        if (ImGui::Button("OK", ImVec2(ERROR_BUTTON_WIDTH, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void drawPlaceholder()
{
    const char *message = "No ROM loaded. Use File > Open ROM... to start a game.";
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const ImVec2 textSize = ImGui::CalcTextSize(message);
    // Centered in the space below the menu bar
    const ImVec2 position(viewport->Pos.x + (viewport->Size.x - textSize.x) / 2.0f,
                          viewport->Pos.y + menuBarHeight + (viewport->Size.y - menuBarHeight - textSize.y) / 2.0f);
    ImGui::GetBackgroundDrawList()->AddText(position, PLACEHOLDER_TEXT_COLOR, message);
}

// Stretch the Game Boy screen to fill
// the whole window below the menu bar
SDL_Rect getScreenRect()
{
    int32_t outputWidth = 0;
    int32_t outputHeight = 0;
    SDL_GetRendererOutputSize(renderer, &outputWidth, &outputHeight);

    const int32_t top = static_cast<int32_t>(menuBarHeight * ImGui::GetIO().DisplayFramebufferScale.y);

    SDL_Rect rect;
    rect.x = 0;
    rect.y = top;
    rect.w = outputWidth;
    rect.h = std::max(0, outputHeight - top);
    return rect;
}

int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "Failed to initalize SDL\n";
        return ExitSDLInitFailed;
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0)
    {
        SDL_AudioSpec audioSpec{};
        audioSpec.freq = AUDIO_SAMPLE_RATE;
        audioSpec.format = AUDIO_S16SYS;
        audioSpec.channels = AUDIO_CHANNELS;
        audioSpec.samples = AUDIO_DEVICE_BUFFER_SAMPLES;
        audioDevice = SDL_OpenAudioDevice(nullptr, 0, &audioSpec, nullptr, 0);
        if (audioDevice != 0) SDL_PauseAudioDevice(audioDevice, 0);
        else std::cerr << "Audio output unavailable: " << SDL_GetError() << "\n";
    }

    window = SDL_CreateWindow("gbemu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    if (window == nullptr)
    {
        std::cerr << "Failed to create SDL window\n";
        return ExitWindowFailed;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr)
    {
        std::cerr << "Failed to create SDL renderer\n";
        return ExitRendererFailed;
    }

    // The texture is the Game Boy's native resolution and
    // gets scaled up with nearest-neighbor filtering
    gbScreenTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                        GAMEBOY_WIDTH, GAMEBOY_HEIGHT);
    if (gbScreenTexture == nullptr)
    {
        std::cerr << "Failed to create SDL texture\n";
        return ExitTextureFailed;
    }
    SDL_SetTextureScaleMode(gbScreenTexture, SDL_ScaleModeNearest);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // A ROM and CGB boot ROM can still be passed on
    // the command line, but neither is required
    if (argc > BOOT_ROM_ARGUMENT) cgbBootROM = readFile(argv[BOOT_ROM_ARGUMENT]);
    if (argc > ROM_ARGUMENT) openROM(argv[ROM_ARGUMENT]);

    bool windowSizedForMenuBar = false;

    while (!shouldExit)
    {
        processSDLEvents();

        // Pause the game while the file browser is open
        if (emulator && !fileBrowser.isOpen())
        {
            try
            {
                emulator->runFrame(&updateScreenTexture);
                queueAudio();
            }
            catch (const std::exception &e)
            {
                closeROM();
                showError("Emulation stopped: " + std::string(e.what()));
            }
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        drawMenuBar();

        std::string chosenPath;
        if (fileBrowser.draw(chosenPath))
        {
            openROM(chosenPath);
        }

        drawErrorPopup();
        if (!emulator) drawPlaceholder();

        ImGui::Render();

        // Grow the window once so the menu bar
        // doesn't eat into the Game Boy screen
        if (!windowSizedForMenuBar && menuBarHeight > 0.0f)
        {
            SDL_SetWindowSize(window, WIDTH, HEIGHT + static_cast<int32_t>(menuBarHeight));
            windowSizedForMenuBar = true;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Opaque black behind everything
        SDL_RenderClear(renderer);

        if (emulator)
        {
            SDL_Rect screenRect = getScreenRect();
            SDL_RenderCopy(renderer, gbScreenTexture, nullptr, &screenRect);
        }

        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    closeROM();

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyTexture(gbScreenTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    if (audioDevice != 0) SDL_CloseAudioDevice(audioDevice);
    SDL_Quit();

    return 0;
}
