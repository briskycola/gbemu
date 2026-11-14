#include "MainWindow.hpp"
#include "Emulator.hpp"
#include "File.hpp"
#include "FrameBuffer.hpp"
#include "ScreenWidget.hpp"
#include <QAction>
#include <QApplication>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QIODevice>
#include <QKeySequence>
#include <QMediaDevices>
#include <QMenuBar>
#include <QMessageBox>
#include <QTimer>
#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <utility>

std::string MainWindow::getSaveFilename(const std::string &filename) { return filename + ".battery"; }

bool MainWindow::fileExists(const std::string &filename)
{
    std::ifstream file(filename.c_str(), std::ios::binary);
    return file.good();
}

void MainWindow::saveBatteryState(Emulator *emulator, const std::string &filename)
{
    if (!emulator) return;
    const std::vector<uint8_t> cartridgeRAM = emulator->getCartridgeRAM();
    if (cartridgeRAM.empty()) return;
    // ROM files are raw bytes, so always open them in binary mode.
    std::ofstream outputFile(getSaveFilename(filename), std::ios::binary);
    std::copy(cartridgeRAM.begin(), cartridgeRAM.end(), std::ostreambuf_iterator<char>(outputFile));
}

std::vector<uint8_t> MainWindow::loadState(const std::string &filename)
{
    const std::string saveFile = getSaveFilename(filename);
    return fileExists(saveFile) ? readFile(saveFile) : std::vector<uint8_t>{};
}

MainWindow::MainWindow(std::vector<uint8_t> bootROM, QWidget *parent)
    : QMainWindow(parent), cgbBootROM(std::move(bootROM))
{
    setWindowTitle("gbemu");
    resize(display::DEFAULT_WIDTH, display::DEFAULT_HEIGHT);
    screen = new ScreenWidget(this);
    setCentralWidget(screen);

    // Use the same compact menu bar padding on every platform
    // (top, right, bottom, left).
    menuBar()->setStyleSheet("QMenuBar::item { padding: 3px 8px 2px 8px; }");
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *openAction = fileMenu->addAction(tr("&Open ROM..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openROMDialog);
    QAction *closeAction = fileMenu->addAction(tr("&Close ROM"));
    connect(closeAction, &QAction::triggered, this, &MainWindow::closeROM);
    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

    setUpAudioOutput();
    // Follow the system default output, e.g. when headphones are plugged in.
    mediaDevices = new QMediaDevices(this);
    connect(mediaDevices, &QMediaDevices::audioOutputsChanged, this, [this]
        {
            if (QMediaDevices::defaultAudioOutput().id() != audioDeviceId) setUpAudioOutput();
        });

    timer = new QTimer(this);
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, this, &MainWindow::runFrame);
    frameClock.start();
    timer->start(1);
}

MainWindow::~MainWindow()
{
    timer->stop();
    closeROM();
    if (audioSink) audioSink->stop();
}

void MainWindow::loadInitialROM(const QString &filename) { if (!filename.isEmpty()) openROM(filename); }

// (Re)creates the audio sink on the current default output device.
// Playback restarts from the queued samples on the next frame.
void MainWindow::setUpAudioOutput()
{
    if (audioSink) audioSink->stop();
    audioSink.reset();
    audioOutput = nullptr;
    audioStarted = false;

    const QAudioDevice audioDevice = QMediaDevices::defaultAudioOutput();
    audioDeviceId = audioDevice.id();
    QAudioFormat format;
    format.setSampleRate(apu::AUDIO_SAMPLE_RATE);
    format.setChannelCount(mainwindow::AUDIO_CHANNELS);
    format.setSampleFormat(QAudioFormat::Int16);
    if (audioDevice.isNull())
    {
        qWarning() << "No default audio output device is available";
        return;
    }
    if (!audioDevice.isFormatSupported(format))
    {
        qWarning() << "Default audio device does not support 48 kHz, stereo, signed 16-bit audio";
        return;
    }
    audioSink = std::make_unique<QAudioSink>(audioDevice, format, this);
    audioSink->setBufferSize(apu::AUDIO_SAMPLE_RATE / 10 * mainwindow::AUDIO_CHANNELS * sizeof(int16_t));
    connect(audioSink.get(), &QAudioSink::stateChanged, this,
        [this](QAudio::State state)
        {
            if (state == QAudio::StoppedState && audioSink->error() != QAudio::NoError)
                qWarning() << "Audio output stopped with error:" << audioSink->error();
        });
}

void MainWindow::showError(const QString &message)
{
    std::cerr << message.toStdString() << "\n";
    QMessageBox::critical(this, tr("gbemu"), message);
}

void MainWindow::closeROM()
{
    if (!emulator) return;
    saveBatteryState(emulator.get(), currentROMPath);
    emulator.reset();
    currentROMPath.clear();
    if (audioSink) audioSink->stop();
    audioOutput = nullptr;
    audioStarted = false;
    pendingAudio.clear();
    screen->setEmulator(nullptr);
    screen->clear();
    setWindowTitle("gbemu");
}

bool MainWindow::openROM(const QString &filename)
{
    const std::string path = filename.toStdString();
    const std::vector<uint8_t> romData = readFile(path);
    if (romData.size() < mainwindow::MINIMUM_ROM_SIZE)
    {
        showError(tr("Could not load ROM: %1").arg(filename));
        return false;
    }
    if (emulator) saveBatteryState(emulator.get(), currentROMPath);

    std::unique_ptr<Emulator> newEmulator;
    try
    {
        newEmulator = std::make_unique<Emulator>(romData, loadState(path), cgbBootROM);
    }
    catch (const std::exception &error)
    {
        showError(tr("Could not load ROM: %1").arg(QString::fromStdString(error.what())));
        return false;
    }

    closeROM();
    emulator = std::move(newEmulator);
    currentROMPath = path;
    screen->setEmulator(emulator.get());
    screen->clear();
    setWindowTitle(tr("gbemu - %1").arg(QFileInfo(filename).fileName()));
    screen->setFocus();
    return true;
}

void MainWindow::openROMDialog()
{
    const QString directory = currentROMPath.empty()
        ? QDir::currentPath() : QFileInfo(QString::fromStdString(currentROMPath)).absolutePath();
    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Open ROM"), directory,
        tr("Game Boy ROMs (*.gb *.gbc *.sgb);;All files (*)"));
    if (!filename.isEmpty()) openROM(filename);
}

void MainWindow::runFrame()
{
    if (!emulator) return;

    const qint64 now = frameClock.nsecsElapsed();
    if (now < nextFrameDeadline) return;
    if (nextFrameDeadline == 0) nextFrameDeadline = now;

    try
    {
        emulator->runFrame([this](const FrameBuffer &buffer) { screen->updateFrame(buffer); });
        std::vector<int16_t> newSamples = emulator->takeAudioSamples();
        if (!newSamples.empty())
        {
            pendingAudio.insert(pendingAudio.end(), newSamples.begin(), newSamples.end());
            const size_t maxSamples = mainwindow::MAX_QUEUED_AUDIO_BYTES / sizeof(int16_t);
            if (pendingAudio.size() > maxSamples)
                pendingAudio.erase(pendingAudio.begin(), pendingAudio.end() - maxSamples);
        }
        // Start with a small cushion so the sink does not enter an
        // underrun state before the next emulation frame arrives.
        if (!audioStarted && audioSink && pendingAudio.size() >= apu::AUDIO_SAMPLE_RATE / 20 * mainwindow::AUDIO_CHANNELS)
        {
            audioOutput = audioSink->start();
            audioStarted = true;
            if (audioSink->error() != QAudio::NoError)
                qWarning() << "Unable to start audio output:" << audioSink->error();
        }
        if (audioStarted && audioOutput && !pendingAudio.empty())
        {
            const qint64 bytesAvailable = audioSink->bytesFree();
            const qint64 bytesPending = static_cast<qint64>(pendingAudio.size() * sizeof(int16_t));
            const qint64 audioFrameBytes = mainwindow::AUDIO_CHANNELS * sizeof(int16_t);
            const qint64 bytesToWrite = (std::min(bytesAvailable, bytesPending) / audioFrameBytes)
                * audioFrameBytes;
            if (bytesToWrite > 0)
            {
                const qint64 bytesWritten = audioOutput->write(
                    reinterpret_cast<const char *>(pendingAudio.data()), bytesToWrite);
                if (bytesWritten > 0)
                    pendingAudio.erase(pendingAudio.begin(),
                        pendingAudio.begin() + bytesWritten / sizeof(int16_t));
            }
        }
        nextFrameDeadline += mainwindow::FRAME_PERIOD_NS;
        // Do not run a burst of catch-up frames after a dialog or a slow frame.
        if (nextFrameDeadline <= frameClock.nsecsElapsed())
            nextFrameDeadline = frameClock.nsecsElapsed() + mainwindow::FRAME_PERIOD_NS;
    }
    catch (const std::exception &error)
    {
        closeROM();
        showError(tr("Emulation stopped: %1").arg(QString::fromStdString(error.what())));
    }
}
