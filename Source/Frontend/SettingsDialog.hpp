#pragma once

#include "Frontend/Settings.hpp"
#include <QDialog>
#include <array>
#include <functional>

class KeyBindingButton;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QSlider;
class QStackedWidget;

// The Settings window: a list of categories on the left, the selected
// category's page on the right, and OK / Cancel / Apply at the bottom.
class SettingsDialog final : public QDialog
{
    public:
        enum class Page
        {
            Emulation,
            BIOS,
            Video,
            Audio,
            Input,
        };

        // apply is called with the edited settings on OK or Apply and
        // returns whether they could be applied (saved).
        SettingsDialog(const Settings &settings, std::function<bool(const Settings&)> apply,
                       QWidget *parent = nullptr);

        void showPage(Page page);

    private:
        std::function<bool(const Settings&)> apply;
        QListWidget *categories;
        QStackedWidget *pages;
        QLineEdit *gameBoyBootROMEdit;
        QLineEdit *gameBoyColorBootROMEdit;
        QLineEdit *superGameBoyBootROMEdit;
        QComboBox *hardwareModelBox;
        QComboBox *fastForwardSpeedBox;
        QCheckBox *pauseInBackgroundBox;
        QComboBox *windowScaleBox;
        QComboBox *scalingBox;
        QComboBox *gameBoyColorsBox;
        QCheckBox *superGameBoyBorderBox;
        QComboBox *audioDeviceBox;
        QSlider *volumeSlider;
        QCheckBox *mutedBox;
        std::array<KeyBindingButton *, input::BUTTON_COUNT> buttonKeyButtons{};
        KeyBindingButton *fastForwardKeyButton;

        QWidget* createBIOSPage(const Settings &settings);
        QWidget* createEmulationPage(const Settings &settings);
        QWidget* createVideoPage(const Settings &settings);
        QWidget* createAudioPage(const Settings &settings);
        QWidget* createInputPage(const Settings &settings);

        // The Game Boy buttons' names, in the order of GBButton
        static std::array<QString, input::BUTTON_COUNT> buttonNames();

        // Explains which buttons (or the fast-forward key) share a key, if
        // any. Returns whether each has a key of its own.
        bool checkButtonKeys(const Settings &edited);

        // Lists "System default" and every connected output, selecting the
        // device with the given ID. A chosen device that is unplugged stays
        // listed under its saved name, so applying doesn't forget it.
        void listAudioDevices(const QString &selectedID, const QString &selectedName);
        void browseForBootROM(QLineEdit *edit, Console console);

        // The settings as currently shown in the window
        Settings editedSettings() const;

        // Checks the chosen boot ROMs and applies the settings.
        // Returns false, after explaining why, if they are not valid.
        bool applyChanges();
};
