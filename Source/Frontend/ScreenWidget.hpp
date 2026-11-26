#pragma once

#include "Core/Common/Common.hpp"
#include "Core/IO/Input.hpp"
#include "Frontend/Settings.hpp"
#include <QImage>
#include <QRect>
#include <QWidget>
#include <array>
#include <cstdint>

class Emulator;
class FrameBuffer;
class QKeyEvent;
class QPaintEvent;

// Draws the Game Boy screen and turns key presses into button presses.
class ScreenWidget final : public QWidget
{
    private:
        QImage image;
        Emulator *emulator = nullptr;
        ScreenScaling scaling = ScreenScaling::Fit;
        // The colors of the four shades, used while recoloring an original
        // Game Boy game
        std::array<uint32_t, 4> shadeColors{};
        bool recolorShades = false;
        bool showSuperGameBoyBorder = true;

        static bool tryToGetGBButton(int key, GBButton &button);

        // The RGB colors of each Game Boy color choice, lightest first
        static std::array<uint32_t, 4> colorsFor(GameBoyColors colors);

        // Where the picture is drawn in the widget
        QRect pictureRect() const;

    protected:
        void paintEvent(QPaintEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;
        void keyReleaseEvent(QKeyEvent *event) override;

    public:
        explicit ScreenWidget(QWidget *parent = nullptr);

        void setEmulator(Emulator *value);
        void clear();
        void updateFrame(const FrameBuffer &buffer);

        // Applies the video settings. They show from the next frame on.
        void applySettings(const Settings &settings);

        // Whether the game runs as an original Game Boy, whose four shades
        // are shown in the chosen Game Boy colors
        void setRecolorShades(bool value);
};
