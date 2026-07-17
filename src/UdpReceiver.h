// UDP receiver: a single worker thread sits in recvfrom() forever and
// hands every parsed Tick to the TickBuffer. The wire format is supplied
// by the user (see CLAUDE.md "Live transport"); ParseUdpPacket() is a
// well-marked TODO until the spec lands. Everything around it is
// ADK-faithful.

#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "TickBuffer.h"

class UdpReceiver
{
public:
    UdpReceiver(TickBuffer& buffer);
    ~UdpReceiver();

    // bindIp may be "0.0.0.0" (== INADDR_ANY) or a specific NIC IPv4.
    bool Start(const std::string& bindIp, uint16_t port);
    void Stop();

    bool IsRunning() const { return m_running.load(); }
    uint64_t PacketsReceived() const { return m_packets.load(); }
    uint64_t PacketsRejected() const { return m_rejected.load(); }

private:
    void Run();

    TickBuffer&            m_buffer;
    std::atomic<bool>      m_running{false};
    std::thread            m_thread;
    SOCKET                 m_socket{INVALID_SOCKET};
    std::atomic<uint64_t>  m_packets{0};
    std::atomic<uint64_t>  m_rejected{0};
};

// Parse a single UDP datagram into one or more Ticks.
// Returns the number of ticks emitted (0 on parse failure).
//
// TODO: body remains a stub until the user supplies the wire-format spec
// (see docs/spec/ — to be added). Wiring is finished; this is the only
// piece of UDP code that is feed-specific.
int ParseUdpPacket(const uint8_t* data, size_t len, Tick* out, size_t maxOut);
