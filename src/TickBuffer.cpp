#include <vector>
#include <mutex>

class TickBuffer {
public:
    TickBuffer(size_t maxSize) : maxSize(maxSize) {}

    void addTick(double price, long volume) {
        std::lock_guard<std::mutex> lock(mutex);
        if (ticks.size() >= maxSize) {
            ticks.erase(ticks.begin()); // Remove the oldest tick
        }
        ticks.push_back({price, volume});
    }

    std::vector<std::pair<double, long>> getTicks() {
        std::lock_guard<std::mutex> lock(mutex);
        return ticks;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        ticks.clear();
    }

private:
    size_t maxSize;
    std::vector<std::pair<double, long>> ticks;
    std::mutex mutex;
};


