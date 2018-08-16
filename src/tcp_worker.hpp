#ifndef TCP_WORKER_HPP_
#define TCP_WORKER_HPP_

#include "janosh_thread.hpp"
#include "shared_pointers.hpp"

namespace janosh {

class TcpWorker : public JanoshThread {
  socket_ptr socket_;

public:
  explicit TcpWorker(socket_ptr socket);
  void run();
  bool connected();
};


} /* namespace janosh */

#endif /* TCP_WORKER_HPP_ */
