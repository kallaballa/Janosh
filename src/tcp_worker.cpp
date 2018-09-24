/*
 * janosh_thread.cpp
 *
 *  Created on: Feb 2, 2014
 *      Author: elchaschab
 */

#include "tcp_worker.hpp"
#include "exception.hpp"
#include "database_thread.hpp"
//#include "flusher_thread.hpp"
//#include "cache_thread.hpp"
#include "janosh.hpp"
#include "tracker.hpp"
#include "cache.hpp"

namespace janosh {

TcpWorker::TcpWorker(nnsocket_ptr socket) :
    JanoshThread("TcpWorker"),
    socket_(socket),
    connected_(true) {
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

  if(req.doTransaction_)
     cmdline += "-x ";

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
  std::ostringstream sso;

  try {
    Request req;
    bool cacheable = false;
    bool cachehit = false;
    bool result = false;

    string response;
    char* buf = NULL;
    size_t num;
    try {
      num = socket_->recv(&buf, NN_MSG, 0);
    } catch(std::exception& ex) {
      LOG_DEBUG_STR("End of request chain");
      setResult(false);
      //socket_->shutdown(0);
      return;
    }
    std::stringstream response_stream;
    response_stream.write(buf, num);
    read_request(req, response_stream);
    nn::freemsg(buf);

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
      if (!req.command_.empty()) {
        if(req.command_ == "trigger") {
          req.command_ = "set";
          req.runTriggers_ = true;
        }

        Tracker::setDoPublish(req.runTriggers_);
        JanoshThreadPtr dt(new DatabaseThread(req, sso));
        dt->runSynchron();
        result = dt->result();

        sso << "__JANOSH_EOF\n" <<  std::to_string(result ? 0 : 1) << '\n';

        if (!result) {
          //shutdown(socket_);
          setResult(false);
        }
      } else {
        sso << "__JANOSH_EOF\n" <<  std::to_string(0) << '\n';
      }

      socket_->send(sso.str().c_str(), sso.str().size(), 0);
    //  stream_->close();
      //JanoshThreadPtr flusher(new FlusherThread(stream_, out_buf, false));
      //flusher->runSynchron();
//     shutdown(socket_);
    } else {
      //JanoshThreadPtr cacheWriter(new CacheThread(stream_));
      //cacheWriter->runSynchron();
//      shutdown(socket_);
    }
    setResult(true);
  } catch (std::exception& ex) {
    janosh::printException(ex);
    setResult(false);
    connected_ = false;
    sso << "__JANOSH_EOF\n" <<  std::to_string(1) << '\n';
    socket_->send(sso.str().c_str(), sso.str().size(), 0);
  //  shutdown(socket_);
  }
}

bool TcpWorker::connected() {
  return connected_;
}
} /* namespace janosh */
