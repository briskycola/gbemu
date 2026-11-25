#include "Settings.hpp"
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
    static constexpr const char *HARDWARE_MODEL          = "hardwareModel";

    // Values of HARDWARE_MODEL
    static constexpr const char *MODEL_AUTOMATIC = "automatic";
    static constexpr const char *MODEL_GAME_BOY  = "gameBoy";
    static constexpr const char *MODEL_GAME_BOY_COLOR = "gameBoyColor";
}

const QString& Settings::bootROM(bool gameBoyColor) const
{
    return gameBoyColor ? gameBoyColorBootROM : gameBoyBootROM;
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
    const QString model = root.value(settings::HARDWARE_MODEL).toString();
    if (model == settings::MODEL_GAME_BOY) loaded.hardwareModel = HardwareModel::GameBoy;
    else if (model == settings::MODEL_GAME_BOY_COLOR) loaded.hardwareModel = HardwareModel::GameBoyColor;
    else loaded.hardwareModel = HardwareModel::Automatic;
    return loaded;
}

bool Settings::save() const
{
    QJsonObject bootROMs;
    bootROMs[settings::GAME_BOY_BOOT_ROM] = gameBoyBootROM;
    bootROMs[settings::GAME_BOY_COLOR_BOOT_ROM] = gameBoyColorBootROM;

    QJsonObject root;
    root[settings::BOOT_ROMS] = bootROMs;
    switch (hardwareModel)
    {
        case HardwareModel::GameBoy: root[settings::HARDWARE_MODEL] = settings::MODEL_GAME_BOY; break;
        case HardwareModel::GameBoyColor: root[settings::HARDWARE_MODEL] = settings::MODEL_GAME_BOY_COLOR; break;
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
