// HTTP/REST backfill transport. Mirrors the QT idiom: a worker thread
// issues an HTTP GET, parses the response into Quotation bars, calls
// SymbolTable::ReplaceBars, then PostMessage's WM_USER_STREAMING_UPDATE
// so AmiBroker re-calls GetQuotesEx and our blend pulls in the new bars.
//
// Endpoint URL, query parameters, auth, and response format are all
// user-supplied (see CLAUDE.md "Backfill transport"). RequestBackfill()
// remains a clearly-marked stub until that spec lands; surrounding
// plumbing is already correct.

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <string>

#include "SymbolTable.h"

class Backfill
{
public:
    Backfill(SymbolTable& symbolTable, HWND& hAmiBrokerWnd);
    ~Backfill();

    // Kick off an async HTTP request for `pszTicker` at `nPeriodSec`.
    // No-op if the same (symbol,periodicity) is already in flight.
    // Returns true if a request was queued, false otherwise.
    bool Request(const char* pszTicker, int nPeriodSec);

    // Has the most recent backfill for this (symbol,periodicity) returned?
    // Maps directly onto the ADK's IsBackfillComplete export.
    bool IsComplete(const char* pszTicker, int nPeriodSec);

    // Drain in-flight requests and stop the worker.
    void Shutdown();

private:
    struct Job { std::string ticker; int periodSec; };

    void WorkerLoop();
    void ProcessJob(const Job& job);

    SymbolTable&              m_symbolTable;
    HWND&                     m_hAmiBrokerWnd; // referenced from Plugin.cpp

    std::mutex                m_mu;
    std::condition_variable   m_cv;
    std::deque<Job>           m_queue;
    std::unordered_set<std::string> m_inFlight;     // "TICKER#period"
    std::unordered_set<std::string> m_complete;     // "TICKER#period"

    std::atomic<bool>         m_running{false};
    std::thread               m_worker;
};

// Issue the HTTP GET and parse response bars into `outBars`.
// Returns true on success; false on HTTP/parse error (the caller should
// log & retry on a backoff).
//
// TODO: body is a stub until the user supplies the HTTP backfill spec.
bool RequestBackfill(const std::string& ticker, int nPeriodSec,
                     CQuoteArray& outBars);
