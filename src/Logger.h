// Daily-rolling file logger. Self-contained (no spdlog dependency) so the
// build is single-step on a fresh Windows clone — see docs/logging.html for
// the design intent. Thread-safe: one std::mutex serialises all writes.

#pragma once

#include <cstdarg>

namespace logger
{
    enum Level { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4 };

    // Initialise the log file in %LOCALAPPDATA%\AmibrokerUDPData\logs.
    // Safe to call repeatedly (e.g. on every Notify(REASON_SETTINGS_CHANGE)).
    void Init(int level);

    // Flush + close. Called from Release().
    void Shutdown();

    void SetLevel(int level);
    bool ShouldLog(int level);

    void Logf(int level, const char* fmt, ...);
}

#define UDP_LOG_TRACE(...) do { if (logger::ShouldLog(logger::Trace)) logger::Logf(logger::Trace, __VA_ARGS__); } while (0)
#define UDP_LOG_DEBUG(...) do { if (logger::ShouldLog(logger::Debug)) logger::Logf(logger::Debug, __VA_ARGS__); } while (0)
#define UDP_LOG_INFO(...)  do { if (logger::ShouldLog(logger::Info))  logger::Logf(logger::Info,  __VA_ARGS__); } while (0)
#define UDP_LOG_WARN(...)  do { if (logger::ShouldLog(logger::Warn))  logger::Logf(logger::Warn,  __VA_ARGS__); } while (0)
#define UDP_LOG_ERROR(...) do { if (logger::ShouldLog(logger::Error)) logger::Logf(logger::Error, __VA_ARGS__); } while (0)
