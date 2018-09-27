#ifndef SRC_SEMAPHORE_HPP_
#define SRC_SEMAPHORE_HPP_

#include <mutex>
#include <condition_variable>

namespace janosh {
class Semaphore
{
private:
    std::mutex mutex_;
    std::condition_variable condition_;
    unsigned long count_; // Initialized as locked.

public:
    Semaphore(unsigned long count) : count_(count) {}
    void notify();
    void wait();
    bool try_wait();
    bool would_wait();
};
}

#endif /* SRC_SEMAPHORE_HPP_ */
