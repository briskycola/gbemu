#pragma once

#include "Core/Common/Common.hpp"
#include "Core/IO/Input.hpp"
#include "Frontend/Settings.hpp"
#include <QImage>
#include <QPainter>
#include <QRect>
#include <QString>
#include <QWidget>
#include <array>
#include <cstdint>

class Emulator;
class FrameBuffer;
class QFocusEvent;
class QKeyEvent;
class QPaintEvent;
class QTimer;

namespace display
{
    // A message over the picture shows for 3 seconds
    static constexpr int MESSAGE_MILLISECONDS = 3000;
    // Its text is a 30th of the picture's height, but at least 12 pixels
    static constexpr int MESSAGE_LINES_PER_PICTURE = 30;
    static constexpr int MESSAGE_MIN_FONT_PIXELS   = 12;
    static constexpr int MESSAGE_BACKGROUND_ALPHA  = 190;
}

// Draws the Game Boy screen and turns key presses into button presses,
// using the keys chosen in the settings.
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
        std::array<int, input::BUTTON_COUNT> buttonKeys = input::DEFAULT_BUTTON_KEYS;
        int fastForwardKey = input::DEFAULT_FAST_FORWARD_KEY;
        bool fastForwardHeld = false;
        // The message shown over the picture, and the timer that clears it
        QString message;
        QTimer *messageTimer;

        void paintMessage(QPainter &painter) const;

        // The Game Boy button a key is bound to, if any
        bool tryToGetGBButton(int key, GBButton &button) const;

        // The RGB colors of each Game Boy color choice, lightest first
        static std::array<uint32_t, 4> colorsFor(GameBoyColors colors);

        // Where the picture is drawn in the widget
        QRect pictureRect() const;

    protected:
        void paintEvent(QPaintEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;
        void keyReleaseEvent(QKeyEvent *event) override;
        // Tab would move the focus away instead of reaching the game
        bool focusNextPrevChild(bool next) override;
        // Keys released while another window has the focus never arrive
        void focusOutEvent(QFocusEvent *event) override;

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

        // Whether the fast-forward key is held
        bool isFastForwarding() const { return fastForwardHeld; }

        // Shows a message over the top left of the picture for a few
        // seconds. Lines are separated by "\n".
        void showMessage(const QString &text);
};
