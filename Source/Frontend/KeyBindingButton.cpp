#include "Frontend/KeyBindingButton.hpp"
#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QKeySequence>

KeyBindingButton::KeyBindingButton(int key, QWidget *parent) : QPushButton(parent), key(key)
{
    connect(this, &QPushButton::clicked, this, [this]
        {
            waiting = true;
            setText(tr("Press a key..."));
        });
    showKey();
}

void KeyBindingButton::setKey(int value)
{
    key = normalizedKey(value);
    showKey();
}

QString KeyBindingButton::keyName(int key)
{
    if (key == Qt::Key_Return) return tr("Enter");
    return QKeySequence(key).toString(QKeySequence::NativeText);
}

int KeyBindingButton::normalizedKey(int key)
{
    return key == Qt::Key_Enter ? Qt::Key_Return : key;
}

void KeyBindingButton::showKey() { setText(keyName(key)); }

void KeyBindingButton::stopWaiting()
{
    waiting = false;
    showKey();
}

bool KeyBindingButton::event(QEvent *event)
{
    if (!waiting) return QPushButton::event(event);

    // Shortcuts would otherwise act on the key first
    if (event->type() == QEvent::ShortcutOverride)
    {
        event->accept();
        return true;
    }
    if (event->type() == QEvent::KeyPress)
    {
        const QKeyEvent *keyEvent = static_cast<const QKeyEvent *>(event);
        if (keyEvent->key() != Qt::Key_Escape && keyEvent->key() != Qt::Key_unknown)
            key = normalizedKey(keyEvent->key());
        stopWaiting();
        return true;
    }
    return QPushButton::event(event);
}

void KeyBindingButton::focusOutEvent(QFocusEvent *event)
{
    if (waiting) stopWaiting();
    QPushButton::focusOutEvent(event);
}
