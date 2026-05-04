#ifndef UDP_RECEIVER_H
#define UDP_RECEIVER_H

#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

class UDPReceiver {
public:
    UDPReceiver(const std::string& multicastAddress, int port);
    ~UDPReceiver();

    void start();
    void stop();

private:
    void initSocket();
    void receiveData();
    void processQueue();
    
    std::string multicastAddress;
    int port;
    int sockfd;
    bool running;
    std::thread receiveThread;
    std::mutex queueMutex;
    std::queue<std::string> tickQueue;
};

#endif // UDP_RECEIVER_H
