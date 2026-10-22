#pragma once

// debug.h - Lightweight diagnostic logging helpers.
// Prints to stdout when the app is started from a terminal.
// All output is prefixed with a tag for easy grepping.

#include "fileLogger.h"

#include <QString>
#include <QDateTime>
#include <atomic>
#include <cstdio>

//  Core print function

inline void dbgPrint(const char* tag, const QString& message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
    const QString line = QStringLiteral("[%1] [%2] %3").arg(timestamp, QString::fromUtf8(tag), message);

    fprintf(stdout, "%s\n", qPrintable(line));
    fflush(stdout);

    FileLogger::instance().write(line);
}

//  Verbose mode

// Details too long for the usual output, such as every server and node a
// location's Properties window lists, printed with DBG_VERBOSE. On while
// Settings > App > Logging > Verbose Logging is (AppConfig::verboseLogging()),
// and for a run started with --verbose or -v.
class VerboseLog
{
public:
    [[nodiscard]] static bool isOn()
    {
        return s_setting.load() == true || s_launchOption.load() == true;
    }
    static void setSetting(const bool on) { s_setting.store(on); }
    static void setLaunchOption(const bool on) { s_launchOption.store(on); }
    [[nodiscard]] static bool launchOption() { return s_launchOption.load(); }

private:
    static inline std::atomic<bool> s_setting{false};
    static inline std::atomic<bool> s_launchOption{false};
};

//  Convenience macros

#define DBG_APP(msg)      dbgPrint("APP     ", (msg))
#define DBG_CLI(msg)      dbgPrint("CLI     ", (msg))
#define DBG_SETTINGS(msg) dbgPrint("SETTINGS", (msg))
#define DBG_STATUS(msg)   dbgPrint("STATUS  ", (msg))
#define DBG_POLL(msg)     dbgPrint("POLL    ", (msg))
// Only in verbose mode; the message is not even built otherwise.
#define DBG_VERBOSE(msg)  (VerboseLog::isOn() == true ? dbgPrint("VERBOSE ", (msg)) : void())

