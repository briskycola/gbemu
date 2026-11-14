#pragma once

#include "Common.hpp"
#include "Input.hpp"
#include <QImage>
#include <QWidget>

class Emulator;
class FrameBuffer;
class QKeyEvent;
class QPaintEvent;

namespace display
{
    static constexpr int PIXEL_SIZE     = 6;
    static constexpr int DEFAULT_WIDTH  = common::GAMEBOY_WIDTH * PIXEL_SIZE;
    static constexpr int DEFAULT_HEIGHT = common::GAMEBOY_HEIGHT * PIXEL_SIZE;
}

// Draws the Game Boy screen and turns key presses into button presses.
class ScreenWidget final : public QWidget
{
    private:
        QImage image;
        Emulator *emulator = nullptr;

        static bool tryToGetGBButton(int key, GBButton &button);

    protected:
        void paintEvent(QPaintEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;
        void keyReleaseEvent(QKeyEvent *event) override;

    public:
        explicit ScreenWidget(QWidget *parent = nullptr);

        void setEmulator(Emulator *value);
        void clear();
        void updateFrame(const FrameBuffer &buffer);
};
