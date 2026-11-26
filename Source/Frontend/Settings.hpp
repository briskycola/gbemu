#pragma once

#include "Core/Emulator.hpp"
#include <QJsonObject>
#include <QString>
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
//         "hardwareModel": "automatic"   (or "gameBoy", "gameBoyColor", "superGameBoy")
//     },
//     "video": {
//         "windowScale": 6,              (1 to 8)
//         "scaling": "fit",              (or "integer", "stretch")
//         "gameBoyColors": "gray",       (or "green", "pocket")
//         "showSuperGameBoyBorder": true
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

namespace video
{
    static constexpr int MIN_WINDOW_SCALE     = 1;
    static constexpr int MAX_WINDOW_SCALE     = 8;
    static constexpr int DEFAULT_WINDOW_SCALE = 6;
}

class Settings
{
    public:
        // Boot ROMs the user supplied. Empty when none has been chosen.
        QString gameBoyBootROM;
        QString gameBoyColorBootROM;
        QString superGameBoyBootROM;

        HardwareModel hardwareModel = HardwareModel::Automatic;

        // The window's size in Game Boy pixels, and how the picture is drawn
        int windowScale = video::DEFAULT_WINDOW_SCALE;
        ScreenScaling scaling = ScreenScaling::Fit;
        GameBoyColors gameBoyColors = GameBoyColors::Gray;
        bool showSuperGameBoyBorder = true;

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
