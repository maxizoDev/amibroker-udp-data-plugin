#include "StdAfx.h"
#include "Settings.h"

Settings g_oSettings;

namespace
{
    constexpr LPCTSTR kSection = _T("Settings");

    // Defaults — chosen to be safe in any environment until the user runs
    // Configure for the first time. Symbol limit defaults to 500 per the
    // locked use-case parameters in CLAUDE.md.
    constexpr LPCTSTR kDefaultBindIp       = _T("0.0.0.0");
    constexpr int     kDefaultPort         = 17000;
    constexpr LPCTSTR kDefaultDealerId     = _T("");
    constexpr LPCTSTR kDefaultBackfillUrl  = _T("");
    constexpr int     kDefaultRefreshMs    = 250;
    constexpr int     kDefaultSymbolLimit  = 500;
    constexpr int     kDefaultLogLevel     = 2;     // info
    constexpr int     kDefaultAutoAdd      = 1;
}

void Settings::LoadFromRegistry()
{
    CWinApp* app = AfxGetApp();

    BindIp            = app->GetProfileString(kSection, _T("BindIp"),       kDefaultBindIp);
    UdpPort           = app->GetProfileInt   (kSection, _T("UdpPort"),      kDefaultPort);
    DealerId          = app->GetProfileString(kSection, _T("DealerId"),     kDefaultDealerId);
    BackfillUrl       = app->GetProfileString(kSection, _T("BackfillUrl"),  kDefaultBackfillUrl);
    RefreshIntervalMs = app->GetProfileInt   (kSection, _T("RefreshMs"),    kDefaultRefreshMs);
    SymbolLimit       = app->GetProfileInt   (kSection, _T("SymbolLimit"),  kDefaultSymbolLimit);
    LogLevel          = app->GetProfileInt   (kSection, _T("LogLevel"),     kDefaultLogLevel);
    AutoAddSymbols    = app->GetProfileInt   (kSection, _T("AutoAdd"),      kDefaultAutoAdd);
}

void Settings::SaveToRegistry() const
{
    CWinApp* app = AfxGetApp();

    app->WriteProfileString(kSection, _T("BindIp"),      BindIp);
    app->WriteProfileInt   (kSection, _T("UdpPort"),     UdpPort);
    app->WriteProfileString(kSection, _T("DealerId"),    DealerId);
    app->WriteProfileString(kSection, _T("BackfillUrl"), BackfillUrl);
    app->WriteProfileInt   (kSection, _T("RefreshMs"),   RefreshIntervalMs);
    app->WriteProfileInt   (kSection, _T("SymbolLimit"), SymbolLimit);
    app->WriteProfileInt   (kSection, _T("LogLevel"),    LogLevel);
    app->WriteProfileInt   (kSection, _T("AutoAdd"),     AutoAddSymbols);
}
