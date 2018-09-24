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

namespace janosh {

TcpWorker::TcpWorker(int maxThreads, socket_ptr socket) :
    JanoshThread("TcpWorker"),
    sendMutex_(new std::mutex()),
    threadSema_(new Semaphore(maxThreads)),
    socket_(socket) {
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
  zmq::message_t* request = new zmq::message_t();

  try {
    if(socket_->connected())
      socket_->recv(request);
    else
      return;
  } catch(std::exception& ex) {
    printException(ex);
    LOG_DEBUG_STR("End of request chain");
    setResult(false);
    //socket_->shutdown(0);
    return;
  }
//  threadSema_->wait();
//  std::thread t([=]() {
    std::ostringstream sso;
    bool result = false;
    try {
      Request req;
      std::stringstream response_stream;
      response_stream.write((char*)request->data(), request->size());
      read_request(req, response_stream);

      LOG_DEBUG_MSG("ppid", req.pinfo_.pid_);
      LOG_DEBUG_MSG("cmdline", req.pinfo_.cmdline_);
      LOG_INFO_STR(reconstructCommandLine(req));

      Janosh* instance = Janosh::getInstance();
      instance->setFormat(req.format_);

      if (!req.command_.empty()) {
        if(req.command_ == "trigger") {
          req.command_ = "set";
          req.runTriggers_ = true;
        }

        Tracker::setDoPublish(req.runTriggers_);
        JanoshThreadPtr dt(new DatabaseThread(req, sso));
        dt->runSynchron();
        result = dt->result();

        sso << "__JANOSH_EOF\n" << std::to_string(result ? 0 : 1) << '\n';

        if (!result) {
          setResult(false);
        }
      } else {
        sso << "__JANOSH_EOF\n" << std::to_string(0) << '\n';
      }
      std::unique_lock<std::mutex>(*sendMutex_.get());
      socket_->send(sso.str().c_str(), sso.str().size(), 0);
      setResult(true);
    } catch (std::exception& ex) {
      janosh::printException(ex);
      setResult(false);
      sso << "__JANOSH_EOF\n" << std::to_string(1) << '\n';
      std::unique_lock<std::mutex>(*sendMutex_.get());
      socket_->send(sso.str().c_str(), sso.str().size(), 0);
    }
//    threadSema_->notify();
//  });
//
//  t.detach();
}

bool TcpWorker::connected() {
  return socket_->connected();
}
} /* namespace janosh */
