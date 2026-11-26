#pragma once

#include "Core/Emulator.hpp"
#include <QJsonObject>
#include <QString>

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
//     }
// }
class Settings
{
    public:
        // Boot ROMs the user supplied. Empty when none has been chosen.
        QString gameBoyBootROM;
        QString gameBoyColorBootROM;
        QString superGameBoyBootROM;

        HardwareModel hardwareModel = HardwareModel::Automatic;

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
};
