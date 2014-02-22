#ifndef DATABASE_THREAD_HPP_
#define DATABASE_THREAD_HPP_

#include <iostream>
#include <boost/asio.hpp>
#include "request.hpp"
#include "janosh_thread.hpp"
#include "shared_pointers.hpp"

namespace janosh {

using std::ostream;
using boost::asio::ip::tcp;

class DatabaseThread : public JanoshThread {
  Request req_;
  socket_ptr socket_;
  ostream_ptr out_;
public:
  DatabaseThread(const Request& req, socket_ptr socket, ostream_ptr out);
  void run();
};
} /* namespace janosh */

#endif /* JANOSH_THREAD_HPP_ */
