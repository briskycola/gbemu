#pragma once

#include <QPushButton>
#include <QString>

class QEvent;
class QFocusEvent;

// A button showing the key a Game Boy button is bound to. Clicking it waits
// for the next key press, which becomes the new key; Escape or clicking
// elsewhere cancels.
class KeyBindingButton final : public QPushButton
{
    private:
        int key;
        bool waiting = false;

        void showKey();
        void stopWaiting();

    protected:
        // Takes every key while waiting, Tab and Enter included, so none of
        // them moves the focus or clicks a button instead
        bool event(QEvent *event) override;
        void focusOutEvent(QFocusEvent *event) override;

    public:
        explicit KeyBindingButton(int key, QWidget *parent = nullptr);

        int getKey() const { return key; }
        void setKey(int value);

        // The key's name as shown to the user, e.g. "W", "Enter" or "Shift"
        static QString keyName(int key);

        // The keypad's Enter counts as the main Enter key
        static int normalizedKey(int key);
};
