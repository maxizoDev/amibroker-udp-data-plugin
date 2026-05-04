#ifndef TICKBUFFER_H
#define TICKBUFFER_H

#include <vector>
#include <cstdint>

struct Tick {
    uint64_t timestamp;
    double bid;
    double ask;
    double price;
};

class TickBuffer {
public:
    TickBuffer(size_t size);
    void addTick(const Tick& tick);
    const std::vector<Tick>& getTicks() const;
    void clear();

private:
    std::vector<Tick> buffer;
    size_t maxSize;
};

#endif // TICKBUFFER_H
