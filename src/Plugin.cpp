// Plugin.cpp - AmiBroker data plug-in entry points.
//
// Implements the ADK data-plug-in contract per the README at
// .adk-reference/README.md (rev 2.11). Layout follows the QT sample
// (Samples/QT/Plugin.cpp): a thin set of exported functions backed by
// the modules in this folder (UdpReceiver, TickBuffer, SymbolTable,
// Backfill, Logger, Settings).
//
// Threading model:
//   - WinMain thread (AmiBroker main):  GetQuotesEx, GetRecentInfo,
//     OnTimerProc (drain TickBuffer, update SymbolTable, post update).
//   - UDP worker thread:                blocks in recvfrom, pushes Tick
//     into TickBuffer; never touches AmiBroker state directly.
//   - Backfill worker thread:           issues HTTP, parses bars into
//     SymbolTable, then PostMessage(WM_USER_STREAMING_UPDATE, ...).
//
// Per ADK README §3.4.1 the plug-in must NOT block on GetQuotesEx /
// GetRecentInfo, so cross-thread notification uses PostMessage rather
// than SendMessage.

#include "StdAfx.h"

#include "../include/Plugin.h"

#include "Backfill.h"
#include "ConfigDlg.h"
#include "Logger.h"
#include "Settings.h"
#include "SymbolTable.h"
#include "TickBuffer.h"
#include "UdpReceiver.h"

#define PLUGIN_NAME    "AmiBroker UDP Data Plug-in"
#define VENDOR_NAME    "Amibroker UDP Data Plugin Vendor"
#define PLUGIN_VER     10000
// PIDCODE('U','D','P','1') is a placeholder; the real ID code should be
// requested from amibroker.com per the ADK README to avoid collisions.
#define PLUGIN_PID     PIDCODE('U','D','P','1')

#define THIS_PLUGIN_TYPE PLUGIN_TYPE_DATA

// ---------------------------------------------------------------------
// PluginInfo descriptor
// ---------------------------------------------------------------------
static struct PluginInfo oPluginInfo =
{
    sizeof(struct PluginInfo),
    THIS_PLUGIN_TYPE,
    PLUGIN_VER,
    PLUGIN_PID,
    PLUGIN_NAME,
    VENDOR_NAME,
    0,             // certificate code - 0 for private plug-ins
    527000         // minimum required AmiBroker version (5.27, when
                   // 64-bit AmiDate / float volume / Aux fields landed)
};

// ---------------------------------------------------------------------
// Globals (state shared across exports)
// ---------------------------------------------------------------------
namespace
{
    enum Status { STATUS_WAIT, STATUS_CONNECTED, STATUS_DISCONNECTED, STATUS_SHUTDOWN };

    HWND          g_hAmiBrokerWnd = nullptr;
    int           g_nStatus       = STATUS_WAIT;

    SymbolTable   g_symbolTable;
    TickBuffer*   g_tickBuffer    = nullptr;
    UdpReceiver*  g_udp           = nullptr;
    Backfill*     g_backfill      = nullptr;

    constexpr UINT_PTR kStreamingTimerId = 0xA17B;

    using CQuoteArrayLocal = CArray<struct Quotation, struct Quotation>;
}

// ---------------------------------------------------------------------
// Timer callback - drains TickBuffer, updates SymbolTable, asks AmiBroker
// to re-pull. Mirrors the ODBC sample's PostMessage idiom (asynchronous,
// non-blocking) per ADK README §3.4.1.
// ---------------------------------------------------------------------
static VOID CALLBACK OnTimerProc(HWND, UINT, UINT_PTR idEvent, DWORD)
{
    if (idEvent != kStreamingTimerId || !g_tickBuffer) return;

    static thread_local std::vector<Tick> drained;
    drained.clear();
    g_tickBuffer->DrainInto(drained);

    if (!drained.empty())
    {
        for (const Tick& t : drained) g_symbolTable.ApplyTick(t);
        if (g_hAmiBrokerWnd)
        {
            PostMessage(g_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE, 0, 0);
        }
    }

    if (g_udp && g_udp->IsRunning() && g_nStatus != STATUS_CONNECTED)
    {
        g_nStatus = STATUS_CONNECTED;
    }
}

// ---------------------------------------------------------------------
// BlendQuoteArrays - canonical helper from Samples/QT/Plugin.cpp.
// Merges in-memory bars (ours) with what AmiBroker handed in (pQuotes).
// ---------------------------------------------------------------------
static int BlendQuoteArrays(struct Quotation* pQuotes, int /*nPeriodicity*/,
                            int nLastValid, int nSize, CQuoteArrayLocal* pCurQuotes)
{
    int iQty = pCurQuotes->GetSize();
    DATE_TIME_INT nFirstDate = (iQty == 0)
        ? static_cast<DATE_TIME_INT>(-1)
        : pCurQuotes->GetAt(0).DateTime.Date;

    int iStart = nLastValid;
    for (; iStart >= 0; --iStart)
    {
        if (pQuotes[iStart].DateTime.Date < nFirstDate) break;
    }
    iStart++;

    int iSrc = 0;
    if (iQty > nSize)
    {
        iStart = 0;
        iSrc   = iQty - nSize;
    }
    else if (iQty + iStart > nSize)
    {
        memmove(pQuotes,
                pQuotes + iQty + iStart - nSize,
                sizeof(Quotation) * (nSize - iQty));
        iStart = nSize - iQty;
        iSrc   = 0;
    }

    int iNumQuotes = (std::min)(nSize - iStart, iQty - iSrc);
    if (iNumQuotes > 0)
    {
        memcpy(pQuotes + iStart, pCurQuotes->GetData() + iSrc,
               iNumQuotes * sizeof(Quotation));
    }
    else
    {
        iNumQuotes = 0;
    }
    return iStart + iNumQuotes;
}

// ---------------------------------------------------------------------
// Required: GetPluginInfo
// ---------------------------------------------------------------------
PLUGINAPI int GetPluginInfo(struct PluginInfo* pInfo)
{
    *pInfo = oPluginInfo;
    return TRUE;
}

// ---------------------------------------------------------------------
// Required: Init / Release
// ---------------------------------------------------------------------
PLUGINAPI int Init(void)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    g_oSettings.LoadFromRegistry();
    logger::Init(g_oSettings.LogLevel);

    UDP_LOG_INFO("Plugin Init: %s v%d", PLUGIN_NAME, PLUGIN_VER);

    g_tickBuffer = new TickBuffer(/*capacity=*/64 * 1024);
    g_udp        = new UdpReceiver(*g_tickBuffer);
    g_backfill   = new Backfill(g_symbolTable, g_hAmiBrokerWnd);

    return 1;
}

PLUGINAPI int Release(void)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    UDP_LOG_INFO("Plugin Release");

    if (g_udp)        { g_udp->Stop();        delete g_udp;        g_udp = nullptr; }
    if (g_backfill)   { g_backfill->Shutdown(); delete g_backfill; g_backfill = nullptr; }
    if (g_tickBuffer) { delete g_tickBuffer;  g_tickBuffer = nullptr; }

    g_symbolTable.Clear();
    logger::Shutdown();
    return 1;
}

// ---------------------------------------------------------------------
// Required: Configure
// ---------------------------------------------------------------------
PLUGINAPI int Configure(LPCTSTR /*pszPath*/, struct InfoSite* pSite)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    CConfigDlg dlg;
    dlg.m_pSite = pSite;
    if (dlg.DoModal() == IDOK)
    {
        // Settings already persisted by the dialog's OnOK; reapply runtime
        // bits that depend on them (rebind UDP socket if port/IP changed).
        if (g_udp)
        {
            g_udp->Stop();
            g_udp->Start(static_cast<LPCSTR>(g_oSettings.BindIp),
                         static_cast<uint16_t>(g_oSettings.UdpPort));
        }
        if (g_hAmiBrokerWnd)
        {
            KillTimer(g_hAmiBrokerWnd, kStreamingTimerId);
            SetTimer(g_hAmiBrokerWnd, kStreamingTimerId,
                     static_cast<UINT>(g_oSettings.RefreshIntervalMs),
                     reinterpret_cast<TIMERPROC>(OnTimerProc));
        }
    }
    return 1;
}

// ---------------------------------------------------------------------
// Required: Notify (lifecycle events)
// ---------------------------------------------------------------------
PLUGINAPI int Notify(struct PluginNotification* pn)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    if (!pn) return 0;

    if (pn->nReason & REASON_DATABASE_LOADED)
    {
        if (g_hAmiBrokerWnd == nullptr)
        {
            g_hAmiBrokerWnd = pn->hMainWnd;
            UDP_LOG_INFO("Database loaded; main HWND=%p", (void*)g_hAmiBrokerWnd);

            if (g_udp)
            {
                if (g_udp->Start(static_cast<LPCSTR>(g_oSettings.BindIp),
                                 static_cast<uint16_t>(g_oSettings.UdpPort)))
                {
                    g_nStatus = STATUS_WAIT;
                }
                else
                {
                    g_nStatus = STATUS_DISCONNECTED;
                }
            }

            SetTimer(g_hAmiBrokerWnd, kStreamingTimerId,
                     static_cast<UINT>(g_oSettings.RefreshIntervalMs),
                     reinterpret_cast<TIMERPROC>(OnTimerProc));
        }
    }

    if (pn->nReason & REASON_DATABASE_UNLOADED)
    {
        UDP_LOG_INFO("Database unloaded");
        if (g_hAmiBrokerWnd) KillTimer(g_hAmiBrokerWnd, kStreamingTimerId);
        if (g_udp) g_udp->Stop();
        g_hAmiBrokerWnd = nullptr;
        g_nStatus = STATUS_WAIT;
    }

    if (pn->nReason & REASON_SETTINGS_CHANGE)
    {
        g_oSettings.LoadFromRegistry();
        logger::SetLevel(g_oSettings.LogLevel);
    }

    if (pn->nReason & REASON_STATUS_RMBCLICK)
    {
        // Status pane right-click menu - mirrors QT's popup.
        CPoint pt; GetCursorPos(&pt);
        CMenu oMenu;
        if (oMenu.LoadMenu(IDR_STATUS_MENU))
        {
            CMenu* poPopup = oMenu.GetSubMenu(0);
            if (poPopup)
            {
                int nResp = poPopup->TrackPopupMenu(
                    TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                    pt.x, pt.y, CWnd::FromHandle(pn->hMainWnd));

                switch (nResp)
                {
                case ID_STATUS_RECONNECT:
                    if (g_udp)
                    {
                        g_udp->Stop();
                        g_udp->Start(static_cast<LPCSTR>(g_oSettings.BindIp),
                                     static_cast<uint16_t>(g_oSettings.UdpPort));
                    }
                    break;
                case ID_STATUS_SHUTDOWN:
                    if (g_udp) g_udp->Stop();
                    g_nStatus = STATUS_SHUTDOWN;
                    break;
                }
            }
        }
    }

    return 1;
}

// ---------------------------------------------------------------------
// Required: SetTimeBase
// Accept all intraday intervals (1 sec ... < 1 day). EOD is not the focus;
// see CLAUDE.md "Periodicities".
// ---------------------------------------------------------------------
PLUGINAPI int SetTimeBase(int nTimeBase)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    if (nTimeBase < 1 || nTimeBase >= 86400) return 0;

    g_symbolTable.RegisterPeriodicity(nTimeBase);
    UDP_LOG_DEBUG("SetTimeBase accepted: %d sec", nTimeBase);
    return 1;
}

// ---------------------------------------------------------------------
// Required: GetQuotesEx (the hot path)
// ---------------------------------------------------------------------
PLUGINAPI int GetQuotesEx(LPCTSTR pszTicker, int nPeriodicity, int nLastValid,
                          int nSize, struct Quotation* pQuotes,
                          GQEContext* /*pContext*/)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    if (!pszTicker || !pszTicker[0] || nSize <= 0) return nLastValid + 1;

    g_symbolTable.RegisterPeriodicity(nPeriodicity);

    // Make sure the symbol exists in our table - matches QT's
    // FindOrAddRecentInfo behaviour, gated by AutoAdd setting.
    g_symbolTable.FindOrAdd(static_cast<LPCSTR>(pszTicker),
                            g_oSettings.AutoAddSymbols != 0);

    // Snapshot in-memory bars and blend with pQuotes.
    CQuoteArrayLocal cur;
    bool have = g_symbolTable.GetBars(static_cast<LPCSTR>(pszTicker),
                                      nPeriodicity, cur);

    if (!have || cur.GetSize() == 0)
    {
        // Trigger an async backfill so future calls have data; meanwhile
        // return whatever AmiBroker already had.
        if (g_backfill)
        {
            g_backfill->Request(static_cast<LPCSTR>(pszTicker), nPeriodicity);
        }
        return nLastValid + 1;
    }

    return BlendQuoteArrays(pQuotes, nPeriodicity, nLastValid, nSize, &cur);
}

// ---------------------------------------------------------------------
// Required (legacy): GetQuotes - wrapper around GetQuotesEx, identical
// to the QT/ODBC samples (Format4 <-> Format5 conversion).
// ---------------------------------------------------------------------
PLUGINAPI int GetQuotes(LPCTSTR pszTicker, int nPeriodicity, int nLastValid,
                        int nSize, struct QuotationFormat4* pQuotes)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    Quotation* pQuote5 = static_cast<Quotation*>(malloc(nSize * sizeof(Quotation)));
    if (!pQuote5) return nLastValid + 1;

    QuotationFormat4* src = pQuotes;
    Quotation*        dst = pQuote5;
    for (int i = 0; i <= nLastValid; ++i, ++src, ++dst) ConvertFormat4Quote(src, dst);

    int nQty = GetQuotesEx(pszTicker, nPeriodicity, nLastValid, nSize, pQuote5, nullptr);

    dst = pQuote5; src = pQuotes;
    for (int i = 0; i < nQty; ++i, ++dst, ++src) ConvertFormat5Quote(dst, src);

    free(pQuote5);
    return nQty;
}

// ---------------------------------------------------------------------
// Required (real-time): GetRecentInfo / GetSymbolLimit
// ---------------------------------------------------------------------
PLUGINAPI struct RecentInfo* GetRecentInfo(LPCTSTR pszTicker)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return g_symbolTable.FindOrAdd(static_cast<LPCSTR>(pszTicker),
                                   g_oSettings.AutoAddSymbols != 0);
}

PLUGINAPI int GetSymbolLimit(void)
{
    return g_oSettings.SymbolLimit > 0 ? g_oSettings.SymbolLimit : 500;
}

// ---------------------------------------------------------------------
// Recommended: GetPluginStatus
// ---------------------------------------------------------------------
PLUGINAPI int GetPluginStatus(struct PluginStatus* status)
{
    if (!status) return 0;
    status->nStructSize = sizeof(*status);

    int symbols = 0; uint64_t ticks = 0;
    g_symbolTable.StatusSnapshot(symbols, ticks);

    switch (g_nStatus)
    {
    case STATUS_WAIT:
        status->nStatusCode = 0x10000000;
        strcpy_s(status->szShortMessage, "WAIT");
        _snprintf_s(status->szLongMessage, _TRUNCATE,
                    "Waiting for first UDP packet (%d symbols, %llu ticks).",
                    symbols, static_cast<unsigned long long>(ticks));
        status->clrStatusColor = RGB(255, 255, 0);
        break;
    case STATUS_CONNECTED:
        status->nStatusCode = 0x00000000;
        strcpy_s(status->szShortMessage, "OK");
        _snprintf_s(status->szLongMessage, _TRUNCATE,
                    "Streaming OK (%d symbols, %llu ticks).",
                    symbols, static_cast<unsigned long long>(ticks));
        status->clrStatusColor = RGB(0, 255, 0);
        break;
    case STATUS_DISCONNECTED:
        status->nStatusCode = 0x20000000;
        strcpy_s(status->szShortMessage, "ERR");
        strcpy_s(status->szLongMessage,
                 "UDP socket bind failed.\n\n"
                 "Check the bind IP/port in File->Database Settings->Configure.");
        status->clrStatusColor = RGB(255, 0, 0);
        break;
    case STATUS_SHUTDOWN:
    default:
        status->nStatusCode = 0x30000000;
        strcpy_s(status->szShortMessage, "DOWN");
        strcpy_s(status->szLongMessage, "Feed shut down. Right-click to reconnect.");
        status->clrStatusColor = RGB(192, 0, 192);
        break;
    }
    return 1;
}

// ---------------------------------------------------------------------
// Recommended: IsBackfillComplete
// ---------------------------------------------------------------------
PLUGINAPI BOOL IsBackfillComplete(LPCTSTR pszTicker)
{
    if (!g_backfill || !pszTicker) return TRUE;
    // Periodicity is not part of the ADK signature; we report complete
    // when ALL outstanding periodicities for the symbol are done. Until
    // the user picks otherwise we treat per-symbol completion against
    // the most-recently-requested periodicity (1-min default).
    return g_backfill->IsComplete(static_cast<LPCSTR>(pszTicker), PERIODICITY_1MIN)
            ? TRUE : FALSE;
}

// ---------------------------------------------------------------------
// Optional: GetExtraData (no fundamentals/EOD-extras supplied).
// ---------------------------------------------------------------------
PLUGINAPI AmiVar GetExtraData(LPCTSTR /*pszTicker*/, LPCTSTR /*pszName*/,
                              int /*nArraySize*/, int /*nPeriodicity*/,
                              void* (* /*pfAlloc*/)(unsigned int))
{
    AmiVar v{};
    v.type = VAR_NONE;
    v.val  = 0;
    return v;
}
