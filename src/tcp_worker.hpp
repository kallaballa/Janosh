#ifndef TCP_WORKER_HPP_
#define TCP_WORKER_HPP_

#include "janosh_thread.hpp"
#include "shared_pointers.hpp"
#include "request.hpp"
#include "semaphore.hpp"

namespace janosh {

class TcpWorker : public JanoshThread {
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
