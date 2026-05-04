// UDPReceiver.cpp
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <queue>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

class UDPReceiver {
private:
    SOCKET sock;
    sockaddr_in localAddr;
    std::queue<std::string> tickQueue;
    std::mutex queueMutex;
    bool running;
    std::thread receiverThread;

    void runReceiver() {
        char buffer[1024];
        int addrLen = sizeof(sockaddr_in);
        sockaddr_in fromAddr;

        while (running) {
            int bytesReceived = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&fromAddr, &addrLen);
            if (bytesReceived > 0) {
                std::string message(buffer, bytesReceived);
                std::lock_guard<std::mutex> lock(queueMutex);
                tickQueue.push(message);
            }
        }
    }

public:
    UDPReceiver(int port, const std::string& multicastIP) {
        // Initialize Winsock
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        // Create a socket
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        // Bind the socket to a local address
        localAddr.sin_family = AF_INET;
        localAddr.sin_port = htons(port);
        localAddr.sin_addr.s_addr = INADDR_ANY;
        bind(sock, (sockaddr*)&localAddr, sizeof(localAddr));

        // Set up multicast
        ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(multicastIP.c_str());
        mreq.imr_interface.s_addr = INADDR_ANY;
        setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));

        running = true;
        receiverThread = std::thread(&UDPReceiver::runReceiver, this);
    }

    ~UDPReceiver() {
        running = false;
        if (receiverThread.joinable()) {
            receiverThread.join();
        }
        closesocket(sock);
        WSACleanup();
    }

    std::string getNextTick() {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (tickQueue.empty()) {
            return ""; // Or handle empty queue case as needed
        }
        std::string tick = tickQueue.front();
        tickQueue.pop();
        return tick;
    }
};

// Example usage:
// int main() {
//     UDPReceiver receiver(12345, "239.255.0.1");
//     // Run the receiver and retrieve ticks from tickQueue
// }
