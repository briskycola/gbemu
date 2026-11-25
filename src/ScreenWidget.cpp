#include "ScreenWidget.hpp"
#include "Emulator.hpp"
#include "FrameBuffer.hpp"
#include <QKeyEvent>
#include <QPainter>
#include <QPaintEvent>
#include <cstdint>

bool ScreenWidget::tryToGetGBButton(int key, GBButton &button)
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

ScreenWidget::ScreenWidget(QWidget *parent) : QWidget(parent)
{
    // Every paint covers the whole widget, so Qt can skip erasing it first.
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(display::DEFAULT_WIDTH, display::DEFAULT_HEIGHT);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ScreenWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    if (image.isNull())
    {
        painter.fillRect(rect(), Qt::black);
        return;
    }
    // Scale the frame (160x144, or 256x224 with a Super Game Boy border) as
    // large as fits while keeping its shape, centered on black. Smooth
    // scaling is off by default, so pixels stay sharp (nearest-neighbor).
    painter.fillRect(rect(), Qt::black);
    const QSize size = image.size().scaled(this->size(), Qt::KeepAspectRatio);
    const QRect target(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
    painter.drawImage(target, image);
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

void ScreenWidget::setEmulator(Emulator *value) { emulator = value; }

void ScreenWidget::clear()
{
    image = QImage(common::GAMEBOY_WIDTH, common::GAMEBOY_HEIGHT, QImage::Format_RGBA8888);
    image.fill(Qt::white);
    update();
}

void ScreenWidget::updateFrame(const FrameBuffer &buffer)
{
    const QSize size(static_cast<int>(buffer.getWidth()), static_cast<int>(buffer.getHeight()));
    if (image.size() != size) image = QImage(size, QImage::Format_RGBA8888);
    for (uint32_t y = 0; y < buffer.getHeight(); ++y)
    {
        auto *row = image.scanLine(static_cast<int>(y));
        for (uint32_t x = 0; x < buffer.getWidth(); ++x)
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
