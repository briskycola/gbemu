#include "Frontend/Settings.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
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
    Settings loaded;
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) return loaded;

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject bootROMs = root.value(settings::BOOT_ROMS).toObject();
    loaded.gameBoyBootROM = bootROMs.value(settings::GAME_BOY_BOOT_ROM).toString();
    loaded.gameBoyColorBootROM = bootROMs.value(settings::GAME_BOY_COLOR_BOOT_ROM).toString();
    loaded.superGameBoyBootROM = bootROMs.value(settings::SUPER_GAME_BOY_BOOT_ROM).toString();
    const QString model = root.value(settings::HARDWARE_MODEL).toString();
    if (model == settings::MODEL_GAME_BOY) loaded.hardwareModel = HardwareModel::GameBoy;
    else if (model == settings::MODEL_GAME_BOY_COLOR) loaded.hardwareModel = HardwareModel::GameBoyColor;
    else if (model == settings::MODEL_SUPER_GAME_BOY) loaded.hardwareModel = HardwareModel::SuperGameBoy;
    else loaded.hardwareModel = HardwareModel::Automatic;
    return loaded;
}

bool Settings::save() const
{
    QJsonObject bootROMs;
    bootROMs[settings::GAME_BOY_BOOT_ROM] = gameBoyBootROM;
    bootROMs[settings::GAME_BOY_COLOR_BOOT_ROM] = gameBoyColorBootROM;
    bootROMs[settings::SUPER_GAME_BOY_BOOT_ROM] = superGameBoyBootROM;

    QJsonObject root;
    root[settings::BOOT_ROMS] = bootROMs;
    switch (hardwareModel)
    {
        case HardwareModel::GameBoy: root[settings::HARDWARE_MODEL] = settings::MODEL_GAME_BOY; break;
        case HardwareModel::GameBoyColor: root[settings::HARDWARE_MODEL] = settings::MODEL_GAME_BOY_COLOR; break;
        case HardwareModel::SuperGameBoy: root[settings::HARDWARE_MODEL] = settings::MODEL_SUPER_GAME_BOY; break;
        default: root[settings::HARDWARE_MODEL] = settings::MODEL_AUTOMATIC; break;
    }

    // QSaveFile writes to a temporary file and swaps it in, so a failed
    // write never leaves a half-written settings file behind
    const QString path = filePath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

bool Settings::operator==(const Settings &other) const
{
    return gameBoyBootROM == other.gameBoyBootROM
        && gameBoyColorBootROM == other.gameBoyColorBootROM
        && hardwareModel == other.hardwareModel;
}
