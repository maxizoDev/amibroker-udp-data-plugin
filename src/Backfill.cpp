#include "StdAfx.h"
#include "Backfill.h"

#include "Logger.h"
#include "Settings.h"

#include <chrono>

Backfill::Backfill(SymbolTable& symbolTable, HWND& hAmiBrokerWnd)
    : m_symbolTable(symbolTable)
    , m_hAmiBrokerWnd(hAmiBrokerWnd)
{
    m_running.store(true);
    m_worker = std::thread(&Backfill::WorkerLoop, this);
}

Backfill::~Backfill()
{
    Shutdown();
}

void Backfill::Shutdown()
{
    if (!m_running.exchange(false)) return;
    m_cv.notify_all();
    if (m_worker.joinable()) m_worker.join();
}

static std::string MakeKey(const std::string& ticker, int nPeriodSec)
{
    char buf[80];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%s#%d", ticker.c_str(), nPeriodSec);
    return buf;
}

bool Backfill::Request(const char* pszTicker, int nPeriodSec)
{
    if (!pszTicker || !pszTicker[0]) return false;

    Job job{ pszTicker, nPeriodSec };
    std::string key = MakeKey(job.ticker, nPeriodSec);

    {
        std::lock_guard<std::mutex> lk(m_mu);
        if (m_inFlight.count(key)) return false;
        m_inFlight.insert(key);
        m_complete.erase(key);
        m_queue.push_back(std::move(job));
    }
    m_cv.notify_one();
    UDP_LOG_DEBUG("Backfill queued: %s @ %ds", pszTicker, nPeriodSec);
    return true;
}

bool Backfill::IsComplete(const char* pszTicker, int nPeriodSec)
{
    if (!pszTicker || !pszTicker[0]) return true;
    std::lock_guard<std::mutex> lk(m_mu);
    // ADK semantics: TRUE = no backfill in progress. We treat "never
    // requested" as TRUE so AmiBroker doesn't spin on symbols we don't
    // back-fill. Only an outstanding job marks us as incomplete.
    return m_inFlight.count(MakeKey(pszTicker, nPeriodSec)) == 0;
}

void Backfill::WorkerLoop()
{
    while (m_running.load())
    {
        Job job;
        {
            std::unique_lock<std::mutex> lk(m_mu);
            m_cv.wait(lk, [this] {
                return !m_running.load() || !m_queue.empty();
            });
            if (!m_running.load()) return;
            job = std::move(m_queue.front());
            m_queue.pop_front();
        }
        ProcessJob(job);
    }
}

void Backfill::ProcessJob(const Job& job)
{
    std::string key = MakeKey(job.ticker, job.periodSec);

    CQuoteArray bars;
    bool ok = RequestBackfill(job.ticker, job.periodSec, bars);

    if (ok)
    {
        m_symbolTable.ReplaceBars(job.ticker.c_str(), job.periodSec, bars);
        UDP_LOG_INFO("Backfill OK: %s @ %ds (%d bars)",
                     job.ticker.c_str(), job.periodSec, bars.GetSize());
    }
    else
    {
        UDP_LOG_WARN("Backfill failed: %s @ %ds (will not retry until next request)",
                     job.ticker.c_str(), job.periodSec);
    }

    {
        std::lock_guard<std::mutex> lk(m_mu);
        m_inFlight.erase(key);
        if (ok) m_complete.insert(key);
    }

    if (m_hAmiBrokerWnd)
    {
        // Asynchronous: ADK README §3.4.1 says do NOT block on
        // GetQuotesEx / GetRecentInfo, so use PostMessage (not SendMessage).
        PostMessage(m_hAmiBrokerWnd, WM_USER_STREAMING_UPDATE, 0, 0);
    }
}

// =====================================================================
// RequestBackfill - HTTP-SPEC-DEPENDENT, currently a stub.
//
// The user's REST endpoint shape will live in a doc under docs/spec/
// and be consumed here. Until then we return false so the surrounding
// plumbing exercises the failure path (logging, in-flight bookkeeping,
// IsBackfillComplete semantics). Wiring is final; only the body of this
// function changes when the spec lands.
//
// Recommended implementation when the spec is supplied:
//   - WinHttpOpen / WinHttpConnect / WinHttpOpenRequest / WinHttpSendRequest
//   - Append DealerId / auth headers from g_oSettings
//   - Parse response (likely JSON or CSV) into Quotation entries
//   - Sort ascending by DateTime, then return true
// =====================================================================
bool RequestBackfill(const std::string& ticker, int nPeriodSec,
                     CQuoteArray& outBars)
{
    static std::atomic<uint64_t> s_logged{0};
    if (s_logged.fetch_add(1) < 4)
    {
        UDP_LOG_DEBUG("RequestBackfill TODO: %s @ %ds (URL='%s')",
                      ticker.c_str(), nPeriodSec,
                      static_cast<LPCSTR>(g_oSettings.BackfillUrl));
    }
    (void)outBars;
    return false;
}
