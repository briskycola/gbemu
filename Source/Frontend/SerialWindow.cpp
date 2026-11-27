#include "Frontend/SerialWindow.hpp"
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
#include <QVBoxLayout>

SerialWindow::SerialWindow(QWidget *parent) : QWidget(parent, Qt::Window)
{
    setWindowTitle(tr("Serial"));
    resize(serial_window::DEFAULT_WIDTH, serial_window::DEFAULT_HEIGHT);

    QLabel *note = new QLabel(tr("Bytes the game sends through the serial port (the link cable)"));
    note->setWordWrap(true);

    output = new QPlainTextEdit;
    output->setReadOnly(true);
    output->setMaximumBlockCount(serial_window::MAX_LINES);
    output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    QPushButton *clearButton = new QPushButton(tr("Clear"));
    connect(clearButton, &QPushButton::clicked, this, &SerialWindow::clear);
    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(clearButton);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(note);
    layout->addWidget(output, 1);
    layout->addLayout(buttons);
}

void SerialWindow::append(const std::vector<uint8_t> &bytes)
{
    if (bytes.empty()) return;

    QString text;
    for (const uint8_t byte : bytes)
    {
        if (byte == '\n' || byte == '\t' || (byte >= ' ' && byte <= '~')) text += QChar(byte);
        else text += QChar(0x00B7); // ·
    }

    // Keep following the end, unless the user scrolled up to read
    QScrollBar *scrollBar = output->verticalScrollBar();
    const bool atEnd = scrollBar->value() == scrollBar->maximum();
    QTextCursor cursor(output->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    if (atEnd) scrollBar->setValue(scrollBar->maximum());
}

void SerialWindow::clear()
{
    output->clear();
}
