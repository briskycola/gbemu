#include "Frontend/Launcher.hpp"
#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QtGlobal>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace launcher
{
    // Set in the copy's environment, so it knows not to start another
    static constexpr const char *DETACHED_COPY_VARIABLE = "GBEMU_DETACHED_COPY";
}

bool Launcher::isDetachedCopy()
{
    return qEnvironmentVariableIsSet(launcher::DETACHED_COPY_VARIABLE);
}

bool Launcher::startDetachedCopy()
{
    QProcess copy;
    copy.setProgram(QCoreApplication::applicationFilePath());
    copy.setArguments(QCoreApplication::arguments().mid(1));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(launcher::DETACHED_COPY_VARIABLE, QStringLiteral("1"));
    copy.setProcessEnvironment(environment);

    // The copy doesn't read from or write to the terminal, which may close
    copy.setStandardInputFile(QProcess::nullDevice());
    copy.setStandardOutputFile(QProcess::nullDevice());
    copy.setStandardErrorFile(QProcess::nullDevice());

#ifdef Q_OS_WIN
    // Leave the terminal's job, as some terminals end every process in their
    // job when they close. A job that doesn't allow leaving it refuses to
    // start the copy, so then it is started inside the job instead.
    copy.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments)
        {
            arguments->flags |= CREATE_BREAKAWAY_FROM_JOB;
        });
    if (copy.startDetached()) return true;
    copy.setCreateProcessArgumentsModifier({});
#endif
    // On Unix, Qt starts it in its own session, away from the terminal's
    return copy.startDetached();
}
