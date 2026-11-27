#pragma once

#include <QWidget>
#include <cstdint>
#include <vector>

class QPlainTextEdit;

namespace serial_window
{
    // Older lines are dropped past this many
    static constexpr int MAX_LINES = 10000;
    static constexpr int DEFAULT_WIDTH  = 560;
    static constexpr int DEFAULT_HEIGHT = 360;
}

// View > Serial: the bytes the game sends through the serial port (the link
// cable). Test ROMs, such as Blargg's, print their results this way. It
// collects them while closed too, so opening it shows everything sent since
// the game started.
// https://gbdev.io/pandocs/Serial_Data_Transfer_(Link_Cable).html
class SerialWindow final : public QWidget
{
    private:
        QPlainTextEdit *output;

    public:
        explicit SerialWindow(QWidget *parent = nullptr);

        // Adds bytes the game sent. Text and line breaks show as they are;
        // other bytes show as "·".
        void append(const std::vector<uint8_t> &bytes);
        void clear();
};
