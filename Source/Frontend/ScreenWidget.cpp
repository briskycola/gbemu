#include "Frontend/ScreenWidget.hpp"
#include "Core/Emulator.hpp"
#include "Core/SGB/SGB.hpp"
#include "Core/Video/FrameBuffer.hpp"
#include "Frontend/KeyBindingButton.hpp"
#include <QKeyEvent>
#include <QPainter>
#include <QPaintEvent>
#include <algorithm>

bool ScreenWidget::tryToGetGBButton(int key, GBButton &button) const
{
    const auto found = std::find(buttonKeys.begin(), buttonKeys.end(), KeyBindingButton::normalizedKey(key));
    if (found == buttonKeys.end()) return false;
    button = static_cast<GBButton>(found - buttonKeys.begin());
    return true;
}

std::array<uint32_t, 4> ScreenWidget::colorsFor(GameBoyColors colors)
{
    switch (colors)
    {
        // The yellow-green of the original Game Boy's screen
        case GameBoyColors::Green: return {0x9BBC0F, 0x8BAC0F, 0x306230, 0x0F380F};
        // The gray-green of the Game Boy Pocket's screen
        case GameBoyColors::Pocket: return {0xC4CFA1, 0x8B956D, 0x4D533C, 0x1F1F1F};
        default: return {shadeToRGB(Color::White), shadeToRGB(Color::LightGray),
                         shadeToRGB(Color::DarkGray), shadeToRGB(Color::Black)};
    }
}

ScreenWidget::ScreenWidget(QWidget *parent) : QWidget(parent), shadeColors(colorsFor(GameBoyColors::Gray))
{
    // Every paint covers the whole widget, so Qt can skip erasing it first.
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(common::GAMEBOY_WIDTH, common::GAMEBOY_HEIGHT);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ScreenWidget::applySettings(const Settings &settings)
{
    scaling = settings.scaling;
    shadeColors = colorsFor(settings.gameBoyColors);
    showSuperGameBoyBorder = settings.showSuperGameBoyBorder;
    // A button held while its key changes would never see its key released
    if (emulator && buttonKeys != settings.buttonKeys)
    {
        for (size_t button = 0; button < input::BUTTON_COUNT; ++button)
            emulator->buttonReleased(static_cast<GBButton>(button));
    }
    buttonKeys = settings.buttonKeys;
    update();
}

void ScreenWidget::setRecolorShades(bool value) { recolorShades = value; }

QRect ScreenWidget::pictureRect() const
{
    switch (scaling)
    {
        case ScreenScaling::Stretch:
            return rect();
        case ScreenScaling::Integer:
        {
            // The largest whole multiple that fits, at least 1
            const int factor = std::max(1, std::min(width() / image.width(), height() / image.height()));
            const QSize size = image.size() * factor;
            return QRect(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
        }
        default:
        {
            // As large as fits while keeping the picture's shape
            const QSize size = image.size().scaled(this->size(), Qt::KeepAspectRatio);
            return QRect(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
        }
    }
}

void ScreenWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (image.isNull()) return;
    // The frame (160x144, or 256x224 with a Super Game Boy border) is
    // centered on black. Smooth scaling is off by default, so pixels stay
    // sharp (nearest-neighbor).
    painter.drawImage(pictureRect(), image);
}

void ScreenWidget::keyPressEvent(QKeyEvent *event)
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

void ScreenWidget::keyReleaseEvent(QKeyEvent *event)
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

bool ScreenWidget::focusNextPrevChild(bool next)
{
    Q_UNUSED(next);
    return false;
}

void ScreenWidget::setEmulator(Emulator *value) { emulator = value; }

void ScreenWidget::clear()
{
    image = QImage(common::GAMEBOY_WIDTH, common::GAMEBOY_HEIGHT, QImage::Format_RGBA8888);
    image.fill(Qt::white);
    update();
}

void ScreenWidget::updateFrame(const FrameBuffer &buffer)
{
    // Without the border, only the Game Boy's screen in the middle of a
    // Super Game Boy frame is shown
    uint32_t left = 0;
    uint32_t top = 0;
    uint32_t width = buffer.getWidth();
    uint32_t height = buffer.getHeight();
    if (!showSuperGameBoyBorder && width == sgb::BORDER_WIDTH && height == sgb::BORDER_HEIGHT)
    {
        left = sgb::GAME_LEFT;
        top = sgb::GAME_TOP;
        width = common::GAMEBOY_WIDTH;
        height = common::GAMEBOY_HEIGHT;
    }

    const QSize size(static_cast<int>(width), static_cast<int>(height));
    if (image.size() != size) image = QImage(size, QImage::Format_RGBA8888);
    for (uint32_t y = 0; y < height; ++y)
    {
        auto *row = image.scanLine(static_cast<int>(y));
        for (uint32_t x = 0; x < width; ++x)
        {
            const uint32_t rgb = recolorShades
                ? shadeColors[static_cast<uint8_t>(buffer.getPixel(left + x, top + y))]
                : buffer.getPixelRGB(left + x, top + y);
            row[4 * x + 0] = static_cast<uchar>((rgb >> 16) & 0xFF); // red
            row[4 * x + 1] = static_cast<uchar>((rgb >> 8) & 0xFF);  // green
            row[4 * x + 2] = static_cast<uchar>(rgb & 0xFF);         // blue
            row[4 * x + 3] = 0xFF;                                  // alpha
        }
    }
    update();
}
