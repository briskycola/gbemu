#pragma once

#include "Core/Audio/APU.hpp"
#include "Core/Common/Common.hpp"
#include "Core/Emulator.hpp"
#include "Core/Video/PPU.hpp"
#include "Frontend/Settings.hpp"
#include "Frontend/SettingsDialog.hpp"
#include <QByteArray>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QString>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class ScreenWidget;
class QAudioSink;
class QIODevice;
class QMediaDevices;
class QAction;
class QAudioDevice;
class QKeyEvent;
class QTimer;

namespace mainwindow
{
    static constexpr size_t MINIMUM_ROM_SIZE      = 0x150;
    static constexpr uint8_t AUDIO_CHANNELS       = 2;
    static constexpr int MAX_QUEUED_AUDIO_BYTES   = apu::AUDIO_SAMPLE_RATE / 10 * AUDIO_CHANNELS * sizeof(int16_t);
    // A DMG/CGB frame is 70224 T-cycles at a 4194304 Hz clock: about 59.73 Hz.
}

// The application window: menus, ROM loading, frame timing and audio output.
class MainWindow final : public QMainWindow
{
    private:
        ScreenWidget *screen;
        QAction *fullScreenAction;
        QAction *pauseAction;
        QTimer *timer;
        std::unique_ptr<QAudioSink> audioSink;
        QMediaDevices *mediaDevices = nullptr;
        QByteArray audioDeviceId;
        QIODevice *audioOutput = nullptr;
        bool audioStarted = false;
        std::unique_ptr<Emulator> emulator;
        std::string currentROMPath;
        Settings settings;
        bool offerRestartOnApply = true;
        QElapsedTimer frameClock;
        qint64 nextFrameDeadline = 0;
        std::vector<int16_t> pendingAudio;

        static std::string getSaveFilename(const std::string &filename);
        static bool fileExists(const std::string &filename);
        static void saveBatteryState(Emulator *emulator, const std::string &filename);
        static std::vector<uint8_t> loadState(const std::string &filename);

        QAudioDevice chosenAudioDevice() const;
        void applyVolume();
        void setUpAudioOutput();
        // A problem the user can fix, like a missing file. gbemu stays open.
        void showError(const QString &message);
        // A problem the emulator can't continue from: saves the game, shows
        // the error and closes gbemu once it is dismissed
        void fatalError(const QString &message);
        // The game's title, console and cartridge, shown when it starts
        static QString describeGame(const CartridgeInfo &info, Console console);
        void updateWindowTitle();
        void closeROM();
        bool openROM(const QString &filename);
        void openROMDialog();
        void runFrame();

        // Opens the Settings window on the given page
        void openSettings(SettingsDialog::Page page);
        // Saves new settings; returns whether that worked
        bool applySettings(const Settings &newSettings);
        void offerRestart();

        // Sizes the window so the picture is scale times the Game Boy's screen
        void resizeToScale(int scale);
        void setFullScreen(bool fullScreen);

    protected:
        void keyPressEvent(QKeyEvent *event) override;

    public:
        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow() override;

        void loadInitialROM(const QString &filename);

        // Called by std::terminate for an exception nothing caught: shows
        // the error, then ends the program
        [[noreturn]] static void reportUncaughtException();
};
