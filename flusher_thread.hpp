#ifndef FLUSHER_THREAD_HPP_
#define FLUSHER_THREAD_HPP_

#include <boost/asio.hpp>
#include "request.hpp"
#include "janosh_thread.hpp"

namespace janosh {

using boost::asio::ip::tcp;

class FlusherThread : public JanoshThread {
  tcp::socket* socket_;
  boost::asio::streambuf* out_buf_;
  bool cacheable_;
public:
  FlusherThread(tcp::socket* s, boost::asio::streambuf* out_buf, bool cacheable);
  void run();
};
} /* namespace janosh */

#endif /* FLUSHER_THREAD_HPP_ */
