#pragma once

// Starting gbemu starts a copy of itself that runs on its own, and the first
// process exits at once, so the shell that started it gets its prompt back.
// The copy never depends on the terminal: it doesn't read from or write to
// it, it runs in its own session on Unix, and on Windows outside the
// terminal's job, so closing the terminal doesn't end it.
class Launcher
{
    public:
        // Whether this process is that copy
        static bool isDetachedCopy();

        // Starts the copy with the same arguments. Returns whether it started.
        // Needs a QCoreApplication.
        static bool startDetachedCopy();
};
