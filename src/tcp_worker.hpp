#ifndef TCP_WORKER_HPP_
#define TCP_WORKER_HPP_

#include "janosh_thread.hpp"
#include "shared_pointers.hpp"
#include "request.hpp"

namespace janosh {

class TcpWorker : public JanoshThread {
  iostream_ptr stream_;
  Request readRequest();

public:
  explicit TcpWorker(iostream_ptr stream);
  void run();
  bool connected();
};


} /* namespace janosh */

#endif /* TCP_WORKER_HPP_ */
