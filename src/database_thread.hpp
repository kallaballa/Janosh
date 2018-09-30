#ifndef DATABASE_THREAD_HPP_
#define DATABASE_THREAD_HPP_

#include <iostream>
#include <boost/asio.hpp>
#include "request.hpp"
#include "janosh_thread.hpp"
#include "shared_pointers.hpp"
#include "janosh.hpp"

namespace janosh {

using std::ostream;
using boost::asio::ip::tcp;

class DatabaseThread : public JanoshThread {
  std::shared_ptr<Janosh> janosh_;
  Request req_;
  std::ostream& out_;
public:
  DatabaseThread(std::shared_ptr<Janosh> janosh, const Request& req, std::ostream& out);
  void run();
};
} /* namespace janosh */

#endif /* JANOSH_THREAD_HPP_ */
