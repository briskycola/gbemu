#pragma once

#include "Core/Emulator.hpp"
#include <QJsonObject>
#include <QString>
#include <array>
#include <cstddef>
#include <vector>

// The emulator's user settings, stored as JSON in the per-user
// configuration folder (on Windows, %LOCALAPPDATA%\gbemu\settings.json):
//
// {
//     "bootROMs": {
//         "gameBoy": "C:/path/to/dmg.bin",
//         "gameBoyColor": "C:/path/to/cgb.bin",
//         "superGameBoy": "C:/path/to/sgb.bin"
//     },
//     "emulation": {
//         "hardwareModel": "automatic",  (or "gameBoy", "gameBoyColor", "superGameBoy")
//         "fastForwardSpeed": 4,         (2, 3, 4 or 8 times normal speed)
//         "pauseInBackground": false
//     },
//     "video": {
//         "windowScale": 6,              (1 to 8)
//         "scaling": "fit",              (or "integer", "stretch")
//         "gameBoyColors": "gray",       (or "green", "pocket")
//         "showSuperGameBoyBorder": true
//     },
//     "audio": {
//         "device": "",                  (empty for the system's default output)
//         "deviceName": "",              (the chosen device's name, shown while it is unplugged)
//         "volume": 100,                 (0 to 100)
//         "muted": false
//     },
//     "input": {
//         "up": "W", "down": "S", "left": "A", "right": "D",
//         "a": "P", "b": "O", "select": "Backspace", "start": "Return",
//         "fastForward": "Space"
//     }
// }

// How the picture fills the window: as large as fits keeping its shape,
// only whole multiples of the Game Boy's pixels, or stretched to fill it
enum class ScreenScaling
{
    Fit,
    Integer,
    Stretch,
};

// The four shades an original Game Boy game is shown in. Game Boy Color
// and Super Game Boy games choose their own colors.
enum class GameBoyColors
{
    Gray,
    Green,
    Pocket,
};

namespace emulation
{
    // The speeds fast-forward can run at, in times normal speed
    static constexpr std::array<int, 4> FAST_FORWARD_SPEEDS = {2, 3, 4, 8};
    static constexpr int DEFAULT_FAST_FORWARD_SPEED = 4;
}

namespace video
{
    static constexpr int MIN_WINDOW_SCALE     = 1;
    static constexpr int MAX_WINDOW_SCALE     = 8;
    static constexpr int DEFAULT_WINDOW_SCALE = 6;
}

namespace audio
{
    static constexpr int MAX_VOLUME = 100;
}

namespace input
{
    // The keys of the Game Boy's buttons, in the order of GBButton
    static constexpr size_t BUTTON_COUNT = 8;
    static constexpr std::array<int, BUTTON_COUNT> DEFAULT_BUTTON_KEYS =
    {
        Qt::Key_W, Qt::Key_S, Qt::Key_A, Qt::Key_D,         // Up, Down, Left, Right
        Qt::Key_P, Qt::Key_O, Qt::Key_Backspace, Qt::Key_Return // A, B, Select, Start
    };
    static constexpr int DEFAULT_FAST_FORWARD_KEY = Qt::Key_Space;
}

class Settings
{
    public:
        // Boot ROMs the user supplied. Empty when none has been chosen.
        QString gameBoyBootROM;
        QString gameBoyColorBootROM;
        QString superGameBoyBootROM;

        HardwareModel hardwareModel = HardwareModel::Automatic;
        // How many times normal speed the game runs while fast-forward is held
        int fastForwardSpeed = emulation::DEFAULT_FAST_FORWARD_SPEED;
        // Whether the game pauses while another window is active
        bool pauseInBackground = false;

        // The window's size in Game Boy pixels, and how the picture is drawn
        int windowScale = video::DEFAULT_WINDOW_SCALE;
        ScreenScaling scaling = ScreenScaling::Fit;
        GameBoyColors gameBoyColors = GameBoyColors::Gray;
        bool showSuperGameBoyBorder = true;

        // The audio output's ID, empty for the system default, and its name
        QString audioDevice;
        QString audioDeviceName;
        int volume = audio::MAX_VOLUME;
        bool muted = false;

        // The key of each Game Boy button, indexed by GBButton
        std::array<int, input::BUTTON_COUNT> buttonKeys = input::DEFAULT_BUTTON_KEYS;
        // The key held to fast-forward
        int fastForwardKey = input::DEFAULT_FAST_FORWARD_KEY;

        // The boot ROM path for one console
        const QString& bootROM(Console console) const;

        // The console's name, for messages
        static QString consoleName(Console console);

        // Where the settings file lives
        static QString filePath();

        // Reads the settings file. Missing or unreadable files and
        // missing entries fall back to the defaults.
        static Settings load();

        // Writes the settings file. Returns whether it worked.
        bool save() const;

        // Whether a running game has to restart to switch from these
        // settings to other ones: the BIOS and hardware only take effect
        // when a game starts
        bool needsRestart(const Settings &other) const;

        bool operator==(const Settings &other) const;
        bool operator!=(const Settings &other) const { return !(*this == other); }

    private:
        QJsonObject toJson() const;
        static Settings fromJson(const QJsonObject &root);

        // Where a name is in a list of names, for settings stored as one of
        // several words. Unknown names give the fallback.
        static int nameIndex(const QString &name, const std::vector<QString> &names, int fallback);
};
