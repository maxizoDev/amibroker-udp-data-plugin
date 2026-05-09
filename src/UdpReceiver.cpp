#include "StdAfx.h"
#include "UdpReceiver.h"

#include "Logger.h"

UdpReceiver::UdpReceiver(TickBuffer& buffer)
    : m_buffer(buffer)
{
}

UdpReceiver::~UdpReceiver()
{
    Stop();
}

bool UdpReceiver::Start(const std::string& bindIp, uint16_t port)
{
    if (m_running.load()) return true;

    WSADATA wsa{};
    int wsaErr = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (wsaErr != 0)
    {
        UDP_LOG_ERROR("WSAStartup failed: %d", wsaErr);
        return false;
    }

    m_socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET)
    {
        UDP_LOG_ERROR("socket() failed: %d", WSAGetLastError());
        WSACleanup();
        return false;
    }

    BOOL reuse = TRUE;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    int rcvbuf = 4 * 1024 * 1024; // 4 MiB - generous for bursty feeds
    setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF,
               reinterpret_cast<const char*>(&rcvbuf), sizeof(rcvbuf));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (bindIp.empty() || bindIp == "0.0.0.0")
    {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    else
    {
        if (InetPtonA(AF_INET, bindIp.c_str(), &addr.sin_addr) != 1)
        {
            UDP_LOG_ERROR("InetPton failed for '%s'", bindIp.c_str());
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            WSACleanup();
            return false;
        }
    }

    if (bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        UDP_LOG_ERROR("bind(%s:%u) failed: %d", bindIp.c_str(), port, WSAGetLastError());
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    UDP_LOG_INFO("UDP listening on %s:%u", bindIp.c_str(), port);

    m_running.store(true);
    m_thread = std::thread(&UdpReceiver::Run, this);
    return true;
}

void UdpReceiver::Stop()
{
    if (!m_running.exchange(false)) return;

    if (m_socket != INVALID_SOCKET)
    {
        // Closing the socket from another thread cleanly aborts the
        // pending recvfrom on the worker.
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    if (m_thread.joinable()) m_thread.join();
    WSACleanup();
    UDP_LOG_INFO("UDP receiver stopped");
}

void UdpReceiver::Run()
{
    constexpr int kBufSize = 64 * 1024;        // > MTU, comfortably oversized
    constexpr int kMaxTicksPerPacket = 64;     // raise once the wire spec is known
    std::vector<uint8_t> buf(kBufSize);
    Tick batch[kMaxTicksPerPacket];

    while (m_running.load())
    {
        sockaddr_in from{};
        int fromLen = sizeof(from);

        int n = recvfrom(m_socket,
                         reinterpret_cast<char*>(buf.data()),
                         static_cast<int>(buf.size()),
                         0,
                         reinterpret_cast<sockaddr*>(&from),
                         &fromLen);

        if (n == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (!m_running.load()) break;          // graceful shutdown
            if (err == WSAEINTR) continue;
            UDP_LOG_WARN("recvfrom error: %d", err);
            continue;
        }
        if (n <= 0) continue;

        m_packets.fetch_add(1, std::memory_order_relaxed);

        int produced = ParseUdpPacket(buf.data(), static_cast<size_t>(n),
                                      batch, kMaxTicksPerPacket);
        if (produced <= 0)
        {
            m_rejected.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        for (int i = 0; i < produced; ++i)
        {
            m_buffer.Push(batch[i]);
        }
    }
}

// =====================================================================
// ParseUdpPacket - WIRE-FORMAT-DEPENDENT, currently a stub.
//
// The user's UDP feed spec will be added under docs/spec/ and consumed
// here. Until then we accept the bytes, log a sample, and reject with
// a 0 return so the caller increments PacketsRejected. Crucially the
// surrounding plumbing (socket, threading, buffer, conversion to AmiBroker
// Tick struct) is already correct and will not change when the spec
// lands - only the decoding logic below changes.
// =====================================================================
int ParseUdpPacket(const uint8_t* data, size_t len, Tick* /*out*/, size_t /*maxOut*/)
{
    static std::atomic<uint64_t> s_logged{0};
    // Log only first 8 packets to avoid drowning the log file.
    if (s_logged.fetch_add(1) < 8)
    {
        UDP_LOG_DEBUG("ParseUdpPacket TODO: received %zu bytes (header byte 0x%02X)",
                      len, len > 0 ? data[0] : 0);
    }
    (void)data;
    return 0;
}
