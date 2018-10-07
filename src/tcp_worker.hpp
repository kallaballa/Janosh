#ifndef TCP_WORKER_HPP_
#define TCP_WORKER_HPP_

#include "janosh_thread.hpp"
#include "shared_pointers.hpp"
#include "request.hpp"
#include "semaphore.hpp"
#include "janosh.hpp"

namespace janosh {

class Barrier {
  std::mutex m_;
  std::condition_variable v_;
public:
  void wait() {
    std::unique_lock<std::mutex> lock(m_);
    v_.wait(lock);
  }

  void notifyAll() {
    std::unique_lock<std::mutex> lock(m_);
    v_.notify_all();
  }
};
class TcpWorker : public JanoshThread {
  shared_ptr<Janosh> janosh_;
  shared_ptr<Semaphore> threadSema_;
  zmq::context_t* context_;
  zmq::socket_t socket_;
  Request readRequest();
  Barrier transactionBarrier_;
  static std::mutex tidMutex_;
  static std::string ctid_;


public:
  explicit TcpWorker(int maxThreads, zmq::context_t* context);
  static void setCurrentTransactionID(const string& tid);
  static string getCurrentTransactionID();

  void run();
  bool connected();
};


} /* namespace janosh */

#endif /* TCP_WORKER_HPP_ */
