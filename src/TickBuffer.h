// Thread-safe buffer between the UDP receiver worker thread (single
// producer) and the AmiBroker main thread (single consumer, called from
// OnTimerProc). Bounded so a stalled consumer can't blow memory; oldest
// ticks are dropped on overflow with a counter so we know it happened.
//
// A `Tick` is a one-print event that carries everything we need to update
// a RecentInfo row + append/extend a Quotation bar. The struct intentionally
// stays close to AmiBroker's own field set so per-tick conversion in the
// hot path is just float copies and a packed-date computation.

#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "../include/Plugin.h"   // for AmiDate

struct Tick
{
    char        szSymbol[64]; // null-terminated; AmiBroker's symbol limit is MAX_SYMBOL_LEN=48
    AmiDate     dateTime;     // wall-clock time of the print (uniqueness fixed up later)
    float       fLast;        // last trade price; EMPTY_VAL if not present
    float       fBid;
    float       fAsk;
    float       fOpen;        // session open if the wire packet carries it; else EMPTY_VAL
    float       fHigh;
    float       fLow;
    float       fTradeVol;    // volume of this print
    float       fTotalVol;    // session cumulative volume
    int         iBidSize;
    int         iAskSize;
    uint32_t    uFlags;       // RI_STATUS_* bits derived during parsing
};

class TickBuffer
{
public:
    explicit TickBuffer(size_t capacity);

    // Producer-side: never blocks; drops oldest tick on overflow and
    // increments the drop counter (read via DroppedCount()).
    void Push(const Tick& tick);

    // Consumer-side: drains every queued tick into `out` (appended).
    // Called from the WM_TIMER callback on the AmiBroker main thread.
    void DrainInto(std::vector<Tick>& out);

    size_t Size();
    uint64_t DroppedCount() const;

private:
    const size_t        m_capacity;
    std::mutex          m_mu;
    std::deque<Tick>    m_q;
    std::atomic<uint64_t> m_dropped{0};
};
