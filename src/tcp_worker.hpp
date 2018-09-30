#ifndef TCP_WORKER_HPP_
#define TCP_WORKER_HPP_

#include "janosh_thread.hpp"
#include "shared_pointers.hpp"
#include "request.hpp"
#include "semaphore.hpp"
#include "janosh.hpp"

namespace janosh {

class TcpWorker : public JanoshThread {
  shared_ptr<Janosh> janosh_;
  shared_ptr<Semaphore> threadSema_;
  zmq::context_t* context_;
  zmq::socket_t socket_;
  Request readRequest();

public:
  explicit TcpWorker(int maxThreads, zmq::context_t* context);
  void run();
  bool connected();
};


} /* namespace janosh */

#endif /* TCP_WORKER_HPP_ */
