#include "StdAfx.h"
#include "TickBuffer.h"

TickBuffer::TickBuffer(size_t capacity)
    : m_capacity(capacity)
{
}

void TickBuffer::Push(const Tick& tick)
{
    std::lock_guard<std::mutex> lk(m_mu);
    if (m_q.size() >= m_capacity)
    {
        m_q.pop_front();
        m_dropped.fetch_add(1, std::memory_order_relaxed);
    }
    m_q.push_back(tick);
}

void TickBuffer::DrainInto(std::vector<Tick>& out)
{
    std::lock_guard<std::mutex> lk(m_mu);
    out.reserve(out.size() + m_q.size());
    while (!m_q.empty())
    {
        out.push_back(m_q.front());
        m_q.pop_front();
    }
}

size_t TickBuffer::Size()
{
    std::lock_guard<std::mutex> lk(m_mu);
    return m_q.size();
}

uint64_t TickBuffer::DroppedCount() const
{
    return m_dropped.load(std::memory_order_relaxed);
}
