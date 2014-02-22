/*
 * janosh_thread.cpp
 *
 *  Created on: Feb 2, 2014
 *      Author: elchaschab
 */

#include <boost/asio.hpp>

#include "flusher_thread.hpp"
#include "janosh.hpp"
#include "commands.hpp"
#include "cache.hpp"
#include "exception.hpp"

namespace janosh {

FlusherThread::FlusherThread(tcp::socket* s, boost::asio::streambuf* out_buf, bool cacheable) :
    JanoshThread("Flusher"),
    socket_(s),
    out_buf_(out_buf),
    cacheable_(cacheable) {
}

void FlusherThread::run() {
  Cache* cache = Cache::getInstance();
  try {
    LOG_DEBUG_MSG("sending", out_buf_->size());
    if(cacheable_) {
      LOG_DEBUG_STR("updating cache");
      cache->setData(boost::asio::buffer_cast<const char*>(out_buf_->data()), out_buf_->size());
    }
    boost::asio::write(*socket_, *out_buf_);
  } catch(std::exception& ex) {
    janosh::printException(ex);
  }
  try {
    LOG_DEBUG_MSG("Closing socket", socket);
    socket_->shutdown(boost::asio::socket_base::shutdown_both);
    socket_->close();
  } catch (std::exception& ex) {
    janosh::printException(ex);
  }
}
} /* namespace janosh */
