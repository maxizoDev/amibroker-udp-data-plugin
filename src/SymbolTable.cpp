#include "StdAfx.h"
#include "SymbolTable.h"

#include "Logger.h"
#include "Settings.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace
{
    std::string ToLower(const char* s)
    {
        std::string r(s ? s : "");
        std::transform(r.begin(), r.end(), r.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return r;
    }
}

SymbolTable::SymbolTable() = default;

void SymbolTable::RegisterPeriodicity(int nPeriodSec)
{
    std::lock_guard<std::mutex> lk(m_mu);
    if (std::find(m_activePeriodicities.begin(), m_activePeriodicities.end(),
                  nPeriodSec) == m_activePeriodicities.end())
    {
        m_activePeriodicities.push_back(nPeriodSec);
        // Backfill existing symbols with an empty array for the new periodicity.
        for (Entry* e : m_entries)
        {
            (void)GetPP(*e, nPeriodSec);
        }
    }
}

SymbolTable::Entry* SymbolTable::FindEntry(const char* pszTicker)
{
    auto it = m_byTicker.find(ToLower(pszTicker));
    return (it == m_byTicker.end()) ? nullptr : it->second;
}

SymbolTable::PerPeriodicity* SymbolTable::GetPP(Entry& e, int nPeriodSec)
{
    for (auto& pp : e.periodicities)
    {
        if (pp.nPeriodSec == nPeriodSec) return &pp;
    }
    e.periodicities.push_back({});
    e.periodicities.back().nPeriodSec = nPeriodSec;
    return &e.periodicities.back();
}

RecentInfo* SymbolTable::FindOrAdd(const char* pszTicker, bool autoAdd)
{
    if (!pszTicker || !pszTicker[0]) return nullptr;

    std::lock_guard<std::mutex> lk(m_mu);

    if (Entry* e = FindEntry(pszTicker)) return &e->info;
    if (!autoAdd) return nullptr;

    if (static_cast<int>(m_entries.size()) >= g_oSettings.SymbolLimit)
    {
        UDP_LOG_WARN("Symbol limit (%d) reached; refusing to add '%s'",
                     g_oSettings.SymbolLimit, pszTicker);
        return nullptr;
    }

    Entry* e = new Entry{};
    e->info.nStructSize = sizeof(RecentInfo);
    strncpy_s(e->info.Name, pszTicker, _TRUNCATE);
    e->info.fLast = e->info.fOpen = e->info.fHigh = e->info.fLow =
    e->info.fBid  = e->info.fAsk = EMPTY_VAL;

    for (int p : m_activePeriodicities) GetPP(*e, p);

    m_entries.push_back(e);
    m_byTicker[ToLower(pszTicker)] = e;

    UDP_LOG_DEBUG("Symbol added: %s (#%zu)", pszTicker, m_entries.size());
    return &e->info;
}

DATE_TIME_INT SymbolTable::SecondKey(const AmiDate& dt)
{
    AmiDate copy = dt;
    copy.PackDate.MicroSec = 0;
    copy.PackDate.MilliSec = 0;
    return copy.Date;
}

DATE_TIME_INT SymbolTable::TruncateToPeriodicity(const AmiDate& dt, int nPeriodSec)
{
    AmiDate copy = dt;
    copy.PackDate.MicroSec = 0;
    copy.PackDate.MilliSec = 0;

    if (nPeriodSec >= 1 && nPeriodSec < 60)
    {
        copy.PackDate.Second = (copy.PackDate.Second / nPeriodSec) * nPeriodSec;
    }
    else if (nPeriodSec >= 60 && nPeriodSec < 3600)
    {
        copy.PackDate.Second = 0;
        int minStep = nPeriodSec / 60;
        copy.PackDate.Minute = (copy.PackDate.Minute / minStep) * minStep;
    }
    else if (nPeriodSec >= 3600 && nPeriodSec < 86400)
    {
        copy.PackDate.Second = 0;
        copy.PackDate.Minute = 0;
        int hourStep = nPeriodSec / 3600;
        copy.PackDate.Hour = (copy.PackDate.Hour / hourStep) * hourStep;
    }
    return copy.Date;
}

void SymbolTable::FoldIntoBar(Entry& e, const Tick& t)
{
    for (auto& pp : e.periodicities)
    {
        DATE_TIME_INT barKey = TruncateToPeriodicity(t.dateTime, pp.nPeriodSec);

        const int sz = pp.bars.GetSize();
        Quotation* qt = nullptr;
        if (sz > 0 && pp.bars.GetData()[sz - 1].DateTime.Date == barKey)
        {
            qt = &pp.bars.GetData()[sz - 1];
        }

        const float price = NOT_EMPTY(t.fLast) ? t.fLast :
                            NOT_EMPTY(t.fAsk)  ? t.fAsk  :
                            NOT_EMPTY(t.fBid)  ? t.fBid  : EMPTY_VAL;

        if (qt == nullptr)
        {
            Quotation bar{};
            bar.DateTime.Date = barKey;
            bar.Price = bar.Open = bar.High = bar.Low = price;
            bar.Volume = NOT_EMPTY(t.fTradeVol) ? t.fTradeVol : 0.f;
            bar.OpenInterest = 0.f;
            bar.AuxData1 = bar.AuxData2 = 0.f;
            pp.bars.Add(bar);
        }
        else
        {
            if (NOT_EMPTY(price))
            {
                if (IS_EMPTY(qt->Open))                     qt->Open = price;
                if (IS_EMPTY(qt->High) || price > qt->High) qt->High = price;
                if (IS_EMPTY(qt->Low)  || price < qt->Low)  qt->Low  = price;
                qt->Price = price;
            }
            if (NOT_EMPTY(t.fTradeVol)) qt->Volume += t.fTradeVol;
        }
    }
}

void SymbolTable::ApplyTick(const Tick& tick)
{
    if (!tick.szSymbol[0]) return;

    std::lock_guard<std::mutex> lk(m_mu);
    Entry* e = FindEntry(tick.szSymbol);
    if (!e)
    {
        if (!g_oSettings.AutoAddSymbols) return;
        if (static_cast<int>(m_entries.size()) >= g_oSettings.SymbolLimit) return;

        e = new Entry{};
        e->info.nStructSize = sizeof(RecentInfo);
        strncpy_s(e->info.Name, tick.szSymbol, _TRUNCATE);
        e->info.fLast = e->info.fOpen = e->info.fHigh = e->info.fLow =
        e->info.fBid  = e->info.fAsk = EMPTY_VAL;

        for (int p : m_activePeriodicities) GetPP(*e, p);

        m_entries.push_back(e);
        m_byTicker[ToLower(tick.szSymbol)] = e;
    }

    // Tick-uniqueness rule: per-symbol counter inside one wall-clock second
    // packed into PackDate.MicroSec. Reset whenever the second advances.
    Tick adjusted = tick;
    DATE_TIME_INT key = SecondKey(adjusted.dateTime);
    if (key != e->lastSecondKey)
    {
        e->lastSecondKey         = key;
        e->uTickCountInSecond    = 0;
    }
    adjusted.dateTime.PackDate.MicroSec = e->uTickCountInSecond++ & 0x3FF;

    // Update RecentInfo for the RT quote window.
    RecentInfo& ri = e->info;
    ri.nStructSize = sizeof(RecentInfo);

    bool bChanged = false;
    if (NOT_EMPTY(adjusted.fLast))   { if (ri.fLast != adjusted.fLast) bChanged = true; ri.fLast = adjusted.fLast; ri.nBitmap |= RI_LAST; }
    if (NOT_EMPTY(adjusted.fBid))    { ri.fBid = adjusted.fBid; ri.nBitmap |= RI_BID; }
    if (NOT_EMPTY(adjusted.fAsk))    { ri.fAsk = adjusted.fAsk; ri.nBitmap |= RI_ASK; }
    if (NOT_EMPTY(adjusted.fOpen))   { ri.fOpen = adjusted.fOpen; ri.nBitmap |= RI_OPEN; }
    if (NOT_EMPTY(adjusted.fHigh) || NOT_EMPTY(adjusted.fLow))
    {
        if (NOT_EMPTY(adjusted.fHigh)) ri.fHigh = adjusted.fHigh;
        if (NOT_EMPTY(adjusted.fLow))  ri.fLow  = adjusted.fLow;
        ri.nBitmap |= RI_HIGHLOW;
    }
    if (NOT_EMPTY(adjusted.fTradeVol)) { ri.fTradeVol = adjusted.fTradeVol; ri.iTradeVol = static_cast<int>(adjusted.fTradeVol); ri.nBitmap |= RI_TRADEVOL; }
    if (NOT_EMPTY(adjusted.fTotalVol)) { ri.fTotalVol = adjusted.fTotalVol; ri.iTotalVol = static_cast<int>(adjusted.fTotalVol); ri.nBitmap |= RI_TOTALVOL; }
    ri.iBidSize = adjusted.iBidSize;
    ri.iAskSize = adjusted.iAskSize;

    ri.nDateUpdate = 10000 * adjusted.dateTime.PackDate.Year +
                     100   * adjusted.dateTime.PackDate.Month +
                             adjusted.dateTime.PackDate.Day;
    ri.nTimeUpdate = 10000 * adjusted.dateTime.PackDate.Hour +
                     100   * adjusted.dateTime.PackDate.Minute +
                             adjusted.dateTime.PackDate.Second;
    if (bChanged) { ri.nDateChange = ri.nDateUpdate; ri.nTimeChange = ri.nTimeUpdate; }

    ri.nStatus = RI_STATUS_UPDATE
               | (adjusted.uFlags & RI_STATUS_TRADE)
               | (adjusted.uFlags & RI_STATUS_BIDASK)
               | RI_STATUS_BARSREADY;
    ri.nBitmap |= RI_DATEUPDATE | RI_DATECHANGE;

    FoldIntoBar(*e, adjusted);

    m_tickCounter.fetch_add(1, std::memory_order_relaxed);
}

bool SymbolTable::GetBars(const char* pszTicker, int nPeriodSec, CQuoteArray& pOut)
{
    std::lock_guard<std::mutex> lk(m_mu);
    Entry* e = FindEntry(pszTicker);
    if (!e) return false;
    PerPeriodicity* pp = GetPP(*e, nPeriodSec);
    if (!pp) return false;
    pOut.Copy(pp->bars);
    return true;
}

void SymbolTable::ReplaceBars(const char* pszTicker, int nPeriodSec, const CQuoteArray& src)
{
    std::lock_guard<std::mutex> lk(m_mu);
    Entry* e = FindEntry(pszTicker);
    if (!e) return;
    PerPeriodicity* pp = GetPP(*e, nPeriodSec);
    if (!pp) return;
    pp->bars.Copy(src);
}

int SymbolTable::Count()
{
    std::lock_guard<std::mutex> lk(m_mu);
    return static_cast<int>(m_entries.size());
}

void SymbolTable::Clear()
{
    std::lock_guard<std::mutex> lk(m_mu);
    for (Entry* e : m_entries) delete e;
    m_entries.clear();
    m_byTicker.clear();
}

void SymbolTable::StatusSnapshot(int& outSymbols, uint64_t& outTickCounter) const
{
    std::lock_guard<std::mutex> lk(m_mu);
    outSymbols = static_cast<int>(m_entries.size());
    outTickCounter = m_tickCounter.load(std::memory_order_relaxed);
}
