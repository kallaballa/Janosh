#ifndef TCP_WORKER_HPP_
#define TCP_WORKER_HPP_

#include "janosh_thread.hpp"
#include "shared_pointers.hpp"
#include "request.hpp"
#include "semaphore.hpp"
#include "janosh.hpp"
#include <libsocket/unixclientstream.hpp>
#include <libsocket/exception.hpp>

namespace janosh {
namespace ls = libsocket;

class TcpWorker : public JanoshThread {
  shared_ptr<Janosh> janosh_;
  ls::unix_stream_client& socket_;
  Request readRequest();

public:
  explicit TcpWorker(ls::unix_stream_client& socket);
  void send(const string& msg);
  void receive(string& msg);
  void run();
  bool connected();
};


} /* namespace janosh */

#endif /* TCP_WORKER_HPP_ */
