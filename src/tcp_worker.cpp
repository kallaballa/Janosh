/*
 * janosh_thread.cpp
 *
 *  Created on: Feb 2, 2014
 *      Author: elchaschab
 */

#include "tcp_worker.hpp"
#include "exception.hpp"
#include "database_thread.hpp"
#include "flusher_thread.hpp"
#include "cache_thread.hpp"
#include "janosh.hpp"
#include "tracker.hpp"
#include "cache.hpp"

namespace janosh {

TcpWorker::TcpWorker(socket_ptr socket) :
    JanoshThread("TcpWorker"),
    socket_(socket) {
}

void shutdown(socket_ptr s) {
  if (s != NULL) {
    LOG_DEBUG_MSG("Closing socket", s);
    s->shutdown(boost::asio::socket_base::shutdown_both);
    s->close();
  }
}

void writeResponseHeader(socket_ptr socket, int returnCode) {
  boost::asio::streambuf rc_buf;
  ostream rc_stream(&rc_buf);
  rc_stream << std::to_string(returnCode);
  LOG_DEBUG_MSG("sending", rc_buf.size());
  boost::asio::write(*socket, rc_buf);
}

string reconstructCommandLine(Request& req) {
  string cmdline = "janosh ";

  if(req.verbose_)
    cmdline += "-v ";

  if(req.format_ == janosh::Bash)
    cmdline += "-b ";
  else if(req.format_ == janosh::Json)
    cmdline += "-j ";
  else if(req.format_ == janosh::Raw)
    cmdline += "-r ";

  if(req.runTriggers_)
     cmdline += "-t ";

  if(!req.doTransaction_)
     cmdline += "-n ";

   cmdline += (req.command_ + " ");

   if(!req.vecArgs_.empty()) {
     bool first = true;
     for(const Value& arg : req.vecArgs_) {
       if(!first)
         cmdline+=" ";
       cmdline+= ("\"" + arg.str() + "\"");

       first = false;
     }
     cmdline += " ";
   }

  return cmdline;
}

void TcpWorker::run() {
  try {
    Request req;
    bool cacheable = false;
    bool cachehit = false;
    std::string peerAddr = socket_->remote_endpoint().address().to_string();
    LOG_DEBUG_MSG("accepted", peerAddr);
    LOG_DEBUG_MSG("socket", socket_);

    boost::asio::streambuf response;
    try {
      boost::asio::read_until(*socket_, response, "\n");
    } catch(std::exception& ex) {
      LOG_DEBUG_STR("End of request chain");
      setResult(false);
      shutdown(socket_);
      return;
    }
    std::istream response_stream(&response);

    read_request(req, response_stream);

    LOG_DEBUG_MSG("ppid", req.pinfo_.pid_);
    LOG_DEBUG_MSG("cmdline", req.pinfo_.cmdline_);
    LOG_INFO_STR(reconstructCommandLine(req));

    // only "-j get /." is cached
    cacheable = req.command_ == "get" && req.format_ == janosh::Json && !req.runTriggers_ && req.vecArgs_.size() == 1
        && req.vecArgs_[0].str() == "/.";

    Cache* cache = Cache::getInstance();
    if (!cacheable && (!req.command_.empty() && (req.command_ != "get" && req.command_ != "dump" && req.command_ != "hash" && req.command_ != "size"))) {
      LOG_DEBUG_STR("Invalidating cache");
      cache->invalidate();
    }

    cachehit = cacheable && cache->isValid();
    //FIXME
    cachehit = false;

    LOG_DEBUG_MSG("cacheable", cacheable);
    LOG_DEBUG_MSG("cachehit", cachehit);

    Janosh* instance = Janosh::getInstance();
    instance->setFormat(req.format_);

    if (!cachehit) {
      streambuf_ptr out_buf(new boost::asio::streambuf());
      ostream_ptr out_stream(new ostream(out_buf.get()));

      if (!req.command_.empty()) {
        if(req.command_ == "trigger") {
          req.command_ = "set";
          req.runTriggers_ = true;
        }

        Tracker::setDoPublish(req.runTriggers_);
        bool result;
        if(!req.doTransaction_)
          LOG_DEBUG_STR("Disabling transactions");

        try {
          bool begin = true;
          if(req.doTransaction_)
            begin = instance->beginTransaction();
          assert(begin);
          JanoshThreadPtr dt(new DatabaseThread(req, out_stream));
          dt->runSynchron();

          if(req.doTransaction_)
            instance->endTransaction(true);

          result = dt->result();
        } catch (...) {
          if(req.doTransaction_)
            instance->endTransaction(false);
          throw;
        }
        // report return code
        writeResponseHeader(socket_, result ? 0 : 1);

        if (!result) {
          //shutdown(socket_);
          setResult(false);
        }
      } else {
        writeResponseHeader(socket_, 0);
      }

      (*out_stream) << "__JANOSH_EOF\n";
      out_stream->flush();
      JanoshThreadPtr flusher(new FlusherThread(socket_, out_buf, cacheable));
      flusher->runSynchron();
//     shutdown(socket_);
    } else {
      JanoshThreadPtr cacheWriter(new CacheThread(socket_));
      cacheWriter->runSynchron();
//      shutdown(socket_);
    }
    setResult(true);
  } catch (std::exception& ex) {
    janosh::printException(ex);
    setResult(false);
    socket_->close();
  //  shutdown(socket_);
  }
}

bool TcpWorker::connected() {
  return socket_->is_open();
}
} /* namespace janosh */
