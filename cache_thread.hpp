#ifndef CACHE_THREAD_HPP_
#define CACHE_THREAD_HPP_

#include <iostream>
#include <boost/asio.hpp>
#include "request.hpp"
#include "janosh_thread.hpp"

namespace janosh {

using std::ostream;
using boost::asio::ip::tcp;

class CacheThread : public JanoshThread {
  tcp::socket* socket_;
public:
  CacheThread(tcp::socket* s);
  void run();
};
} /* namespace janosh */

#endif /* CACHE_THREAD_HPP_ */
