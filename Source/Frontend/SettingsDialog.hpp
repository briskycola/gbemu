#pragma once

#include "Frontend/Settings.hpp"
#include <QDialog>
#include <functional>

class QComboBox;
class QLineEdit;
class QListWidget;
class QStackedWidget;

// The Settings window: a list of categories on the left, the selected
// category's page on the right, and OK / Cancel / Apply at the bottom.
class SettingsDialog final : public QDialog
{
    public:
        enum class Page
        {
            BIOS,
            GameBoy,
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

        QWidget* createBIOSPage(const Settings &settings);
        QWidget* createGameBoyPage(const Settings &settings);
        void browseForBootROM(QLineEdit *edit, Console console);

        // The settings as currently shown in the window
        Settings editedSettings() const;

        // Checks the chosen boot ROMs and applies the settings.
        // Returns false, after explaining why, if they are not valid.
        bool applyChanges();
};
