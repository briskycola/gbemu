#pragma once

#include "APU.hpp"
#include "Common.hpp"
#include "PPU.hpp"
#include <QByteArray>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QString>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Emulator;
class ScreenWidget;
class QAudioSink;
class QIODevice;
class QMediaDevices;
class QTimer;

namespace mainwindow
{
    static constexpr size_t MINIMUM_ROM_SIZE      = 0x150;
    static constexpr uint8_t AUDIO_CHANNELS       = 2;
    static constexpr int MAX_QUEUED_AUDIO_BYTES   = apu::AUDIO_SAMPLE_RATE / 10 * AUDIO_CHANNELS * sizeof(int16_t);
    // A DMG/CGB frame is 70224 T-cycles at a 4194304 Hz clock: about 59.73 Hz.
    static constexpr qint64 FRAME_PERIOD_NS       = (1'000'000'000LL * ppu::CLOCKS_PER_FRAME) / common::GAMEBOY_CLOCK_RATE;
}

// The application window: menus, ROM loading, frame timing and audio output.
class MainWindow final : public QMainWindow
{
    private:
        ScreenWidget *screen;
        QTimer *timer;
        std::unique_ptr<QAudioSink> audioSink;
        QMediaDevices *mediaDevices = nullptr;
        QByteArray audioDeviceId;
        QIODevice *audioOutput = nullptr;
        bool audioStarted = false;
        std::unique_ptr<Emulator> emulator;
        std::string currentROMPath;
        std::vector<uint8_t> cgbBootROM;
        QElapsedTimer frameClock;
        qint64 nextFrameDeadline = 0;
        std::vector<int16_t> pendingAudio;

        static std::string getSaveFilename(const std::string &filename);
        static bool fileExists(const std::string &filename);
        static void saveBatteryState(Emulator *emulator, const std::string &filename);
        static std::vector<uint8_t> loadState(const std::string &filename);

        void setUpAudioOutput();
        void showError(const QString &message);
        void closeROM();
        bool openROM(const QString &filename);
        void openROMDialog();
        void runFrame();

    public:
        explicit MainWindow(std::vector<uint8_t> bootROM, QWidget *parent = nullptr);
        ~MainWindow() override;

        void loadInitialROM(const QString &filename);
};
