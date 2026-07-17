#include "StdAfx.h"
#include "Logger.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <shlobj.h>

namespace logger
{
    namespace
    {
        std::mutex      g_mu;
        FILE*           g_fp        = nullptr;
        std::atomic<int> g_level{Info};
        SYSTEMTIME      g_openedFor{};

        void ResolveLogDir(char* out, size_t cap)
        {
            char base[MAX_PATH] = {};
            // CSIDL_LOCAL_APPDATA = %LOCALAPPDATA%; reasonable for a per-user
            // service-style plug-in. Falls back to the AmiBroker working dir
            // if the shell call fails.
            if (FAILED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, base)))
            {
                strncpy_s(base, sizeof(base), ".", _TRUNCATE);
            }
            _snprintf_s(out, cap, _TRUNCATE, "%s\\AmibrokerUDPData\\logs", base);
            SHCreateDirectoryExA(nullptr, out, nullptr);
        }

        void OpenForToday()
        {
            SYSTEMTIME st;
            GetLocalTime(&st);
            g_openedFor = st;

            char dir[MAX_PATH];
            ResolveLogDir(dir, sizeof(dir));

            char path[MAX_PATH];
            _snprintf_s(path, sizeof(path), _TRUNCATE,
                        "%s\\udp-plugin-%04u-%02u-%02u.log",
                        dir, st.wYear, st.wMonth, st.wDay);

            if (g_fp) { fclose(g_fp); g_fp = nullptr; }
            fopen_s(&g_fp, path, "ab");
        }

        bool DayChanged(const SYSTEMTIME& now)
        {
            return now.wYear  != g_openedFor.wYear  ||
                   now.wMonth != g_openedFor.wMonth ||
                   now.wDay   != g_openedFor.wDay;
        }

        const char* LevelName(int level)
        {
            switch (level) {
                case Trace: return "TRACE";
                case Debug: return "DEBUG";
                case Info:  return "INFO ";
                case Warn:  return "WARN ";
                case Error: return "ERROR";
                default:    return "?????";
            }
        }
    }

    void Init(int level)
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_level.store(level);
        if (!g_fp) OpenForToday();
    }

    void Shutdown()
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_fp) { fflush(g_fp); fclose(g_fp); g_fp = nullptr; }
    }

    void SetLevel(int level) { g_level.store(level); }
    bool ShouldLog(int level) { return level >= g_level.load(); }

    void Logf(int level, const char* fmt, ...)
    {
        if (!ShouldLog(level)) return;

        SYSTEMTIME st;
        GetLocalTime(&st);

        std::lock_guard<std::mutex> lk(g_mu);

        if (!g_fp || DayChanged(st)) OpenForToday();
        if (!g_fp) return;

        fprintf(g_fp, "%04u-%02u-%02u %02u:%02u:%02u.%03u %s [%lu] ",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                LevelName(level), GetCurrentThreadId());

        va_list ap;
        va_start(ap, fmt);
        vfprintf(g_fp, fmt, ap);
        va_end(ap);

        fputc('\n', g_fp);
        fflush(g_fp);
    }
}
