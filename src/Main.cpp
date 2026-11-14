#include "Emulator.hpp"
#include "File.hpp"
#include "FrameBuffer.hpp"

#include <QAction>
#include <QApplication>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QAudioSink>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QIODevice>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QTimer>
#include <QMediaDevices>
#include <QDebug>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace constants
{
    constexpr int PIXEL_SIZE = 6;
    constexpr int WIDTH = common::GAMEBOY_WIDTH * PIXEL_SIZE;
    constexpr int HEIGHT = common::GAMEBOY_HEIGHT * PIXEL_SIZE;
    constexpr size_t MINIMUM_ROM_SIZE = 0x150;
    constexpr int ROM_ARGUMENT = 1;
    constexpr int BOOT_ROM_ARGUMENT = 2;
    constexpr uint8_t AUDIO_CHANNELS = 2;
    constexpr int MAX_QUEUED_AUDIO_BYTES = apu::AUDIO_SAMPLE_RATE / 10 * AUDIO_CHANNELS * sizeof(int16_t);
    // A DMG/CGB frame is 70224 T-cycles at a 4194304 Hz clock: about 59.73 Hz.
    constexpr qint64 FRAME_PERIOD_NS = (1'000'000'000LL * ppu::CLOCKS_PER_FRAME) / common::GAMEBOY_CLOCK_RATE;
}

bool tryToGetGBButton(int key, GBButton &button)
{
    switch (key)
    {
        case Qt::Key_W: button = GBButton::Up; return true;
        case Qt::Key_S: button = GBButton::Down; return true;
        case Qt::Key_A: button = GBButton::Left; return true;
        case Qt::Key_D: button = GBButton::Right; return true;
        case Qt::Key_P: button = GBButton::A; return true;
        case Qt::Key_O: button = GBButton::B; return true;
        case Qt::Key_Backspace: button = GBButton::Select; return true;
        case Qt::Key_Return: case Qt::Key_Enter: button = GBButton::Start; return true;
        default: return false;
    }
}

std::string getSaveFilename(const std::string &filename) { return filename + ".battery"; }

bool fileExists(const std::string &filename)
{
    std::ifstream file(filename.c_str(), std::ios::binary);
    return file.good();
}

void saveBatteryState(Emulator *emulator, const std::string &filename)
{
    if (!emulator) return;
    const std::vector<uint8_t> cartridgeRAM = emulator->getCartridgeRAM();
    if (cartridgeRAM.empty()) return;
    // ROM files are raw bytes, so always open them in binary mode.
    std::ofstream outputFile(getSaveFilename(filename), std::ios::binary);
    std::copy(cartridgeRAM.begin(), cartridgeRAM.end(), std::ostreambuf_iterator<char>(outputFile));
}

std::vector<uint8_t> loadState(const std::string &filename)
{
    const std::string saveFile = getSaveFilename(filename);
    return fileExists(saveFile) ? readFile(saveFile) : std::vector<uint8_t>{};
}

class ScreenWidget final : public QOpenGLWidget, protected QOpenGLFunctions
{
    private:
        QImage image;
        Emulator *emulator = nullptr;
        GLuint texture = 0;
        QOpenGLBuffer vertexBuffer{QOpenGLBuffer::VertexBuffer};
        std::unique_ptr<QOpenGLShaderProgram> shader;

        static constexpr char VERTEX_SHADER[] =
            "attribute vec2 position;"
            "attribute vec2 texCoord;"
            "varying vec2 textureCoord;"
            "void main() {"
            "  gl_Position = vec4(position, 0.0, 1.0);"
            "  textureCoord = texCoord;"
            "}";

        static constexpr char FRAGMENT_SHADER[] =
            "uniform sampler2D screenTexture;"
            "varying vec2 textureCoord;"
            "void main() {"
            "  gl_FragColor = texture2D(screenTexture, textureCoord);"
            "}";

    protected:
        void initializeGL() override
        {
            initializeOpenGLFunctions();
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, common::GAMEBOY_WIDTH,
                         common::GAMEBOY_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            shader = std::make_unique<QOpenGLShaderProgram>();
            shader->addShaderFromSourceCode(QOpenGLShader::Vertex, VERTEX_SHADER);
            shader->addShaderFromSourceCode(QOpenGLShader::Fragment, FRAGMENT_SHADER);
            shader->link();

            // Position (x,y) followed by texture coordinate (u,v).
            static constexpr float vertices[] = {
                -1.0f, -1.0f, 0.0f, 1.0f,
                 1.0f, -1.0f, 1.0f, 1.0f,
                -1.0f,  1.0f, 0.0f, 0.0f,
                 1.0f,  1.0f, 1.0f, 0.0f,
            };
            vertexBuffer.create();
            vertexBuffer.bind();
            vertexBuffer.allocate(vertices, sizeof(vertices));
            vertexBuffer.release();
        }

        void paintGL() override
        {
            glViewport(0, 0, static_cast<GLsizei>(width() * devicePixelRatioF()),
                       static_cast<GLsizei>(height() * devicePixelRatioF()));
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            if (!shader || !texture || image.isNull()) return;

            glBindTexture(GL_TEXTURE_2D, texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image.width(), image.height(),
                            GL_RGBA, GL_UNSIGNED_BYTE, image.constBits());

            shader->bind();
            shader->setUniformValue("screenTexture", 0);
            vertexBuffer.bind();
            const int positionLocation = shader->attributeLocation("position");
            const int textureLocation = shader->attributeLocation("texCoord");
            shader->enableAttributeArray(positionLocation);
            shader->setAttributeBuffer(positionLocation, GL_FLOAT, 0, 2, 4 * sizeof(float));
            shader->enableAttributeArray(textureLocation);
            shader->setAttributeBuffer(textureLocation, GL_FLOAT, 2 * sizeof(float), 2, 4 * sizeof(float));
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            shader->disableAttributeArray(positionLocation);
            shader->disableAttributeArray(textureLocation);
            vertexBuffer.release();
            shader->release();
        }

        void keyPressEvent(QKeyEvent *event) override
        {
            if (event->isAutoRepeat() || !emulator) return;
            GBButton button;
            if (tryToGetGBButton(event->key(), button))
            {
                emulator->buttonPressed(button);
                event->accept();
            }
            else QWidget::keyPressEvent(event);
        }

        void keyReleaseEvent(QKeyEvent *event) override
        {
            if (event->isAutoRepeat() || !emulator) return;
            GBButton button;
            if (tryToGetGBButton(event->key(), button))
            {
                emulator->buttonReleased(button);
                event->accept();
            }
            else QWidget::keyReleaseEvent(event);
        }

        void resizeGL(int width, int height) override
        {
            Q_UNUSED(width);
            Q_UNUSED(height);
        }

    public:
        explicit ScreenWidget(QWidget *parent = nullptr) : QOpenGLWidget(parent)
        {
            setFocusPolicy(Qt::StrongFocus);
            setMinimumSize(constants::WIDTH, constants::HEIGHT);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        }

        void setEmulator(Emulator *value) { emulator = value; }

        void clear()
        {
            image = QImage(common::GAMEBOY_WIDTH, common::GAMEBOY_HEIGHT, QImage::Format_RGBA8888);
            image.fill(Qt::white);
            update();
        }

        void updateFrame(const FrameBuffer &buffer)
        {
            if (image.size() != QSize(common::GAMEBOY_WIDTH, common::GAMEBOY_HEIGHT))
                image = QImage(common::GAMEBOY_WIDTH, common::GAMEBOY_HEIGHT, QImage::Format_RGBA8888);
            for (uint32_t y = 0; y < common::GAMEBOY_HEIGHT; ++y)
            {
                auto *row = image.scanLine(static_cast<int>(y));
                for (uint32_t x = 0; x < common::GAMEBOY_WIDTH; ++x)
                {
                    const uint32_t rgb = buffer.getPixelRGB(x, y);
                    row[4 * x + 0] = static_cast<uchar>((rgb >> 16) & 0xFF); // red
                    row[4 * x + 1] = static_cast<uchar>((rgb >> 8) & 0xFF);  // green
                    row[4 * x + 2] = static_cast<uchar>(rgb & 0xFF);         // blue
                    row[4 * x + 3] = 0xFF;                                  // alpha
                }
            }
            update();
        }
};

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

        // (Re)creates the audio sink on the current default output device.
        // Playback restarts from the queued samples on the next frame.
        void setUpAudioOutput()
        {
            if (audioSink) audioSink->stop();
            audioSink.reset();
            audioOutput = nullptr;
            audioStarted = false;

            const QAudioDevice audioDevice = QMediaDevices::defaultAudioOutput();
            audioDeviceId = audioDevice.id();
            QAudioFormat format;
            format.setSampleRate(apu::AUDIO_SAMPLE_RATE);
            format.setChannelCount(constants::AUDIO_CHANNELS);
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
            audioSink->setBufferSize(apu::AUDIO_SAMPLE_RATE / 10 * constants::AUDIO_CHANNELS * sizeof(int16_t));
            connect(audioSink.get(), &QAudioSink::stateChanged, this,
                [this](QAudio::State state)
                {
                    if (state == QAudio::StoppedState && audioSink->error() != QAudio::NoError)
                        qWarning() << "Audio output stopped with error:" << audioSink->error();
                });
        }

        void showError(const QString &message)
        {
            std::cerr << message.toStdString() << "\n";
            QMessageBox::critical(this, tr("gbemu"), message);
        }

        void closeROM()
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

        bool openROM(const QString &filename)
        {
            const std::string path = filename.toStdString();
            const std::vector<uint8_t> romData = readFile(path);
            if (romData.size() < constants::MINIMUM_ROM_SIZE)
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

        void openROMDialog()
        {
            const QString directory = currentROMPath.empty()
                ? QDir::currentPath() : QFileInfo(QString::fromStdString(currentROMPath)).absolutePath();
            const QString filename = QFileDialog::getOpenFileName(
                this, tr("Open ROM"), directory,
                tr("Game Boy ROMs (*.gb *.gbc *.sgb);;All files (*)"));
            if (!filename.isEmpty()) openROM(filename);
        }

        void runFrame()
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
                    const size_t maxSamples = constants::MAX_QUEUED_AUDIO_BYTES / sizeof(int16_t);
                    if (pendingAudio.size() > maxSamples)
                        pendingAudio.erase(pendingAudio.begin(), pendingAudio.end() - maxSamples);
                }
                // Start with a small cushion so the sink does not enter an
                // underrun state before the next emulation frame arrives.
                if (!audioStarted && audioSink && pendingAudio.size() >= apu::AUDIO_SAMPLE_RATE / 20 * constants::AUDIO_CHANNELS)
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
                    const qint64 audioFrameBytes = constants::AUDIO_CHANNELS * sizeof(int16_t);
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
                nextFrameDeadline += constants::FRAME_PERIOD_NS;
                // Do not run a burst of catch-up frames after a dialog or a slow frame.
                if (nextFrameDeadline <= frameClock.nsecsElapsed())
                    nextFrameDeadline = frameClock.nsecsElapsed() + constants::FRAME_PERIOD_NS;
            }
            catch (const std::exception &error)
            {
                closeROM();
                showError(tr("Emulation stopped: %1").arg(QString::fromStdString(error.what())));
            }
        }

    public:
        explicit MainWindow(std::vector<uint8_t> bootROM, QWidget *parent = nullptr)
            : QMainWindow(parent), cgbBootROM(std::move(bootROM))
        {
            setWindowTitle("gbemu");
            resize(constants::WIDTH, constants::HEIGHT);
            screen = new ScreenWidget(this);
            setCentralWidget(screen);

#ifdef Q_OS_WIN
            // Qt's "windows11" style pads menu bar items far more than native
            // Win32 menus; trim it to a typical Windows menu bar height.
            menuBar()->setStyleSheet("QMenuBar::item { padding: 3px 8px 2px 8px; }");
#endif
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

        ~MainWindow() override
        {
            timer->stop();
            closeROM();
            if (audioSink) audioSink->stop();
        }

        void loadInitialROM(const QString &filename) { if (!filename.isEmpty()) openROM(filename); }
};

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setApplicationName("gbemu");
    application.setApplicationDisplayName("gbemu");

    std::vector<uint8_t> bootROM;
    if (argc > constants::BOOT_ROM_ARGUMENT) bootROM = readFile(argv[constants::BOOT_ROM_ARGUMENT]);

    MainWindow window(std::move(bootROM));
    window.show();
    if (argc > constants::ROM_ARGUMENT) window.loadInitialROM(QString::fromLocal8Bit(argv[constants::ROM_ARGUMENT]));
    return application.exec();
}
