#include "Frontend/Settings.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace settings
{
    static constexpr const char *FILE_NAME = "settings.json";

    // JSON keys
    static constexpr const char *BOOT_ROMS               = "bootROMs";
    static constexpr const char *GAME_BOY_BOOT_ROM       = "gameBoy";
    static constexpr const char *GAME_BOY_COLOR_BOOT_ROM = "gameBoyColor";
    static constexpr const char *SUPER_GAME_BOY_BOOT_ROM = "superGameBoy";
    static constexpr const char *EMULATION               = "emulation";
    static constexpr const char *HARDWARE_MODEL          = "hardwareModel";

    // Values of HARDWARE_MODEL
    static constexpr const char *MODEL_AUTOMATIC = "automatic";
    static constexpr const char *MODEL_GAME_BOY  = "gameBoy";
    static constexpr const char *MODEL_GAME_BOY_COLOR = "gameBoyColor";
    static constexpr const char *MODEL_SUPER_GAME_BOY = "superGameBoy";
}

const QString& Settings::bootROM(Console console) const
{
    switch (console)
    {
        case Console::GameBoyColor: return gameBoyColorBootROM;
        case Console::SuperGameBoy: return superGameBoyBootROM;
        default: return gameBoyBootROM;
    }
}

QString Settings::consoleName(Console console)
{
    switch (console)
    {
        case Console::GameBoyColor: return QCoreApplication::translate("Settings", "Game Boy Color");
        case Console::SuperGameBoy: return QCoreApplication::translate("Settings", "Super Game Boy");
        default: return QCoreApplication::translate("Settings", "Game Boy");
    }
}

QString Settings::filePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(settings::FILE_NAME);
}

Settings Settings::load()
{
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) return Settings();
    return fromJson(QJsonDocument::fromJson(file.readAll()).object());
}

bool Settings::save() const
{
    // QSaveFile writes to a temporary file and swaps it in, so a failed
    // write never leaves a half-written settings file behind
    const QString path = filePath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    return file.commit();
}

QJsonObject Settings::toJson() const
{
    QJsonObject bootROMs;
    bootROMs[settings::GAME_BOY_BOOT_ROM] = gameBoyBootROM;
    bootROMs[settings::GAME_BOY_COLOR_BOOT_ROM] = gameBoyColorBootROM;
    bootROMs[settings::SUPER_GAME_BOY_BOOT_ROM] = superGameBoyBootROM;

    QJsonObject emulation;
    switch (hardwareModel)
    {
        case HardwareModel::GameBoy: emulation[settings::HARDWARE_MODEL] = settings::MODEL_GAME_BOY; break;
        case HardwareModel::GameBoyColor: emulation[settings::HARDWARE_MODEL] = settings::MODEL_GAME_BOY_COLOR; break;
        case HardwareModel::SuperGameBoy: emulation[settings::HARDWARE_MODEL] = settings::MODEL_SUPER_GAME_BOY; break;
        default: emulation[settings::HARDWARE_MODEL] = settings::MODEL_AUTOMATIC; break;
    }

    QJsonObject root;
    root[settings::BOOT_ROMS] = bootROMs;
    root[settings::EMULATION] = emulation;
    return root;
}

Settings Settings::fromJson(const QJsonObject &root)
{
    Settings loaded;
    const QJsonObject bootROMs = root.value(settings::BOOT_ROMS).toObject();
    loaded.gameBoyBootROM = bootROMs.value(settings::GAME_BOY_BOOT_ROM).toString();
    loaded.gameBoyColorBootROM = bootROMs.value(settings::GAME_BOY_COLOR_BOOT_ROM).toString();
    loaded.superGameBoyBootROM = bootROMs.value(settings::SUPER_GAME_BOY_BOOT_ROM).toString();

    // Older settings files kept the hardware model at the top level
    const QJsonObject emulation = root.value(settings::EMULATION).toObject();
    const QString model = emulation.contains(settings::HARDWARE_MODEL)
        ? emulation.value(settings::HARDWARE_MODEL).toString()
        : root.value(settings::HARDWARE_MODEL).toString();
    if (model == settings::MODEL_GAME_BOY) loaded.hardwareModel = HardwareModel::GameBoy;
    else if (model == settings::MODEL_GAME_BOY_COLOR) loaded.hardwareModel = HardwareModel::GameBoyColor;
    else if (model == settings::MODEL_SUPER_GAME_BOY) loaded.hardwareModel = HardwareModel::SuperGameBoy;
    else loaded.hardwareModel = HardwareModel::Automatic;
    return loaded;
}

bool Settings::needsRestart(const Settings &other) const
{
    return gameBoyBootROM != other.gameBoyBootROM
        || gameBoyColorBootROM != other.gameBoyColorBootROM
        || superGameBoyBootROM != other.superGameBoyBootROM
        || hardwareModel != other.hardwareModel;
}

// Comparing the JSON compares every setting, so none can be forgotten
bool Settings::operator==(const Settings &other) const
{
    return toJson() == other.toJson();
}
