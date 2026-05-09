// Per-symbol state: RecentInfo (returned to AmiBroker via GetRecentInfo)
// plus a per-periodicity ring of finished Quotation bars (returned to
// AmiBroker via GetQuotesEx after blending with what's already in the
// AmiBroker database). The QT sample stores the bar array per request;
// for our locked use case (~500 symbols, intraday-heavy) we keep them
// resident so the GetQuotesEx hot path is just a memcpy + BlendQuoteArrays.
//
// Concurrency: ApplyTick / GetBars are called from the AmiBroker main
// thread (the WM_TIMER callback drains TickBuffer and updates here, and
// GetQuotesEx/GetRecentInfo are also main-thread). A single mutex is fine.
//
// Bar promotion: each tick is folded into the currently-open bar for each
// active periodicity; when the tick crosses the next interval boundary the
// previous open bar is sealed and a new one started. This is the
// "no on-demand recompression on the GetQuotesEx hot path" rule from
// CLAUDE.md.

#pragma once

#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <mutex>

#include "../include/Plugin.h"   // Quotation, RecentInfo, AmiDate
#include "TickBuffer.h"

using CQuoteArray = CArray<struct Quotation, struct Quotation>;

class SymbolTable
{
public:
    SymbolTable();

    // Registers the periodicity AmiBroker is currently asking about (called
    // from SetTimeBase + GetQuotesEx). Idempotent.
    void RegisterPeriodicity(int nPeriodSec);

    // Look up - or, if AutoAdd, create - a RecentInfo for `pszTicker`.
    // The pointer is stable for the life of the plug-in (we never relocate
    // entries) which matches what AmiBroker expects from GetRecentInfo.
    RecentInfo* FindOrAdd(const char* pszTicker, bool autoAdd);

    // Apply one batched tick: update RecentInfo and fold into open bars.
    // The MicroSec collision-resolution rule is applied here so all bars
    // and RecentInfo timestamps remain unique per (symbol, second).
    void ApplyTick(const Tick& tick);

    // Snapshot the bar array for (symbol, periodicity) into pOut. Returns
    // true if known, false if symbol or periodicity not tracked. Called
    // from GetQuotesEx; must run on the main thread.
    bool GetBars(const char* pszTicker, int nPeriodSec, CQuoteArray& pOut);

    // Replace the entire bar array for (symbol, periodicity) - used by the
    // Backfill module when historical bars arrive from HTTP.
    void ReplaceBars(const char* pszTicker, int nPeriodSec, const CQuoteArray& src);

    // Capacity helpers (return values for GetSymbolLimit etc.)
    int  Count();
    void Clear();

    // Diagnostic snapshot (not thread-safe-strict; for status pane).
    void StatusSnapshot(int& outSymbols, uint64_t& outTickCounter) const;

private:
    struct PerPeriodicity
    {
        int          nPeriodSec = 0;
        CQuoteArray  bars;        // chronological, oldest first
    };

    struct Entry
    {
        RecentInfo  info{};
        std::vector<PerPeriodicity> periodicities;
        // Tick-uniqueness counter: number of ticks already accepted in the
        // current second (PackDate.Second + Year/Month/Day/Hour/Minute).
        uint32_t    uTickCountInSecond = 0;
        DATE_TIME_INT lastSecondKey    = 0;
    };

    Entry* FindEntry(const char* pszTicker);
    PerPeriodicity* GetPP(Entry& e, int nPeriodSec);

    void FoldIntoBar(Entry& e, const Tick& t);
    static DATE_TIME_INT TruncateToPeriodicity(const AmiDate& dt, int nPeriodSec);
    static DATE_TIME_INT SecondKey(const AmiDate& dt);

    mutable std::mutex                  m_mu;
    std::vector<Entry*>                 m_entries;          // stable pointers
    std::unordered_map<std::string, Entry*> m_byTicker;     // case-insensitive key (lower)
    std::vector<int>                    m_activePeriodicities;
    std::atomic<uint64_t>               m_tickCounter{0};
};
