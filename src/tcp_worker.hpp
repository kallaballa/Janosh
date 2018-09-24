#ifndef TCP_WORKER_HPP_
#define TCP_WORKER_HPP_

#include "janosh_thread.hpp"
#include "shared_pointers.hpp"
#include "request.hpp"
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
    void notify() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        ++count_;
        condition_.notify_one();
    }

    void wait() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        while(!count_) // Handle spurious wake-ups.
            condition_.wait(lock);
        --count_;
    }

    bool try_wait() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        if(count_) {
            --count_;
            return true;
        }
        return false;
    }
};

class TcpWorker : public JanoshThread {
  shared_ptr<std::mutex> sendMutex_;
  shared_ptr<Semaphore> threadSema_;
  socket_ptr socket_;
  Request readRequest();

public:
  explicit TcpWorker(int maxThreads, socket_ptr stream);
  void run();
  bool connected();
};


} /* namespace janosh */

#endif /* TCP_WORKER_HPP_ */
