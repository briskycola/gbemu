#include "Frontend/Settings.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

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
    static constexpr const char *VIDEO                   = "video";
    static constexpr const char *WINDOW_SCALE            = "windowScale";
    static constexpr const char *SCALING                 = "scaling";
    static constexpr const char *GAME_BOY_COLORS         = "gameBoyColors";
    static constexpr const char *SHOW_SGB_BORDER         = "showSuperGameBoyBorder";
    static constexpr const char *AUDIO                   = "audio";
    static constexpr const char *AUDIO_DEVICE            = "device";
    static constexpr const char *AUDIO_DEVICE_NAME       = "deviceName";
    static constexpr const char *VOLUME                  = "volume";
    static constexpr const char *MUTED                   = "muted";

    // The words stored for each choice, in the order of its enum
    static const std::vector<QString> HARDWARE_MODEL_NAMES  = {"automatic", "gameBoy", "gameBoyColor", "superGameBoy"};
    static const std::vector<QString> SCALING_NAMES         = {"fit", "integer", "stretch"};
    static const std::vector<QString> GAME_BOY_COLORS_NAMES = {"gray", "green", "pocket"};
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
    emulation[settings::HARDWARE_MODEL] = settings::HARDWARE_MODEL_NAMES[static_cast<int>(hardwareModel)];

    QJsonObject video;
    video[settings::WINDOW_SCALE] = windowScale;
    video[settings::SCALING] = settings::SCALING_NAMES[static_cast<int>(scaling)];
    video[settings::GAME_BOY_COLORS] = settings::GAME_BOY_COLORS_NAMES[static_cast<int>(gameBoyColors)];
    video[settings::SHOW_SGB_BORDER] = showSuperGameBoyBorder;

    QJsonObject root;
    root[settings::BOOT_ROMS] = bootROMs;
    root[settings::EMULATION] = emulation;
    root[settings::VIDEO] = video;

    QJsonObject audio;
    audio[settings::AUDIO_DEVICE] = audioDevice;
    audio[settings::AUDIO_DEVICE_NAME] = audioDeviceName;
    audio[settings::VOLUME] = volume;
    audio[settings::MUTED] = muted;
    root[settings::AUDIO] = audio;
    return root;
}

int Settings::nameIndex(const QString &name, const std::vector<QString> &names, int fallback)
{
    const auto found = std::find(names.begin(), names.end(), name);
    return found == names.end() ? fallback : static_cast<int>(found - names.begin());
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
    loaded.hardwareModel = static_cast<HardwareModel>(nameIndex(
        model, settings::HARDWARE_MODEL_NAMES, static_cast<int>(loaded.hardwareModel)));

    const QJsonObject video = root.value(settings::VIDEO).toObject();
    loaded.windowScale = std::clamp(video.value(settings::WINDOW_SCALE).toInt(loaded.windowScale),
                                    video::MIN_WINDOW_SCALE, video::MAX_WINDOW_SCALE);
    loaded.scaling = static_cast<ScreenScaling>(nameIndex(
        video.value(settings::SCALING).toString(), settings::SCALING_NAMES, static_cast<int>(loaded.scaling)));
    loaded.gameBoyColors = static_cast<GameBoyColors>(nameIndex(
        video.value(settings::GAME_BOY_COLORS).toString(), settings::GAME_BOY_COLORS_NAMES,
        static_cast<int>(loaded.gameBoyColors)));
    loaded.showSuperGameBoyBorder = video.value(settings::SHOW_SGB_BORDER).toBool(loaded.showSuperGameBoyBorder);

    const QJsonObject audio = root.value(settings::AUDIO).toObject();
    loaded.audioDevice = audio.value(settings::AUDIO_DEVICE).toString();
    loaded.audioDeviceName = audio.value(settings::AUDIO_DEVICE_NAME).toString();
    loaded.volume = std::clamp(audio.value(settings::VOLUME).toInt(loaded.volume), 0, audio::MAX_VOLUME);
    loaded.muted = audio.value(settings::MUTED).toBool(loaded.muted);
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
