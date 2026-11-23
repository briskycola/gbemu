#pragma once

#include "Emulator.hpp"
#include <QString>

// The emulator's user settings, stored as JSON in the per-user
// configuration folder (on Windows, %LOCALAPPDATA%\gbemu\settings.json):
//
// {
//     "bootROMs": {
//         "gameBoy": "C:/path/to/dmg.bin",
//         "gameBoyColor": "C:/path/to/cgb.bin"
//     },
//     "hardwareModel": "automatic"
// }
class Settings
{
    public:
        // Boot ROMs the user supplied. Empty when none has been chosen.
        QString gameBoyBootROM;
        QString gameBoyColorBootROM;

        HardwareModel hardwareModel = HardwareModel::Automatic;

        // The boot ROM path for one console
        const QString& bootROM(bool gameBoyColor) const;

        // Where the settings file lives
        static QString filePath();

        // Reads the settings file. Missing or unreadable files and
        // missing entries fall back to the defaults.
        static Settings load();

        // Writes the settings file. Returns whether it worked.
        bool save() const;

        bool operator==(const Settings &other) const;
        bool operator!=(const Settings &other) const { return !(*this == other); }
};
