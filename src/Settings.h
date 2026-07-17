// User-configurable settings for the plug-in. Values live in the registry
// (HKCU\Software\AmiBroker UDP Plugin Vendor\Settings) via AfxGetApp()'s
// profile API, matching the QT / ODBC sample idiom.

#pragma once

#ifndef __AFXWIN_H__
    #error include 'StdAfx.h' before including this file for PCH
#endif

struct Settings
{
    CString BindIp;          // local interface to bind, "0.0.0.0" = INADDR_ANY
    int     UdpPort;         // UDP port the upstream feed sends to
    CString DealerId;        // free-form auth/identifier echoed in HTTP backfill
    CString BackfillUrl;     // base URL for HTTP/REST historical-bar requests
    int     RefreshIntervalMs; // PostMessage(WM_USER_STREAMING_UPDATE) cadence
    int     SymbolLimit;     // max number of streaming symbols (matches GetSymbolLimit)
    int     LogLevel;        // 0=trace 1=debug 2=info 3=warn 4=error
    int     AutoAddSymbols;  // BOOL: auto-create RecentInfo for unknown tickers

    void LoadFromRegistry();
    void SaveToRegistry() const;
};

extern Settings g_oSettings;
