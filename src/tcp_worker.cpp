/*
 * janosh_thread.cpp
 *
 *  Created on: Feb 2, 2014
 *      Author: elchaschab
 */

#include "tcp_worker.hpp"
#include "exception.hpp"
#include "database_thread.hpp"
#include "tracker.hpp"
#include "record.hpp"

namespace janosh {

TcpWorker::TcpWorker(int maxThreads, zmq::context_t* context) :
    JanoshThread("TcpWorker"),
    janosh_(new Janosh()),
    threadSema_(new Semaphore(maxThreads)),
    context_(context),
    socket_(*context_, ZMQ_REP) {
  socket_.connect("inproc://workers");
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
  std::shared_ptr<zmq::message_t> request(new zmq::message_t());
  bool transaction = false;
  while (true) {
    try {
      LOG_DEBUG_STR("Recv start");
      socket_.recv(request.get());
      LOG_DEBUG_STR("Recv end");
    } catch (std::exception& ex) {
      printException(ex);
      LOG_DEBUG_STR("End of request chain");
      setResult(false);
      return;
    }
    string requestData;
    requestData.assign((const char*)request->data(), request->size());
    LOG_DEBUG_STR(requestData);
    if(requestData == "begin") {
      LOG_DEBUG_STR("Begin transaction");
      string reply = "done";
      socket_.send(reply.data(), reply.size());
      transaction = janosh_->beginTransaction();
      assert(transaction);
      LOG_DEBUG_STR("Begin end");
      continue;
    } else if(requestData == "commit") {
      LOG_DEBUG_STR("Commit transaction");
      string reply = "done";
      socket_.send(reply.data(), reply.size());
      if(transaction)
        janosh_->endTransaction(true);
      continue;
    } else if(requestData == "abort") {
      LOG_DEBUG_STR("Abort transaction");
      string reply = "done";
      socket_.send(reply.data(), reply.size());
      if(transaction)
        janosh_->endTransaction(false);
      continue;
    }

    std::ostringstream sso;
    bool result = false;
    try {
      Request req;
      std::stringstream response_stream;
      response_stream.write((char*) request->data(), request->size());
      read_request(req, response_stream);

      LOG_DEBUG_MSG("ppid", req.pinfo_.pid_);
      LOG_DEBUG_MSG("cmdline", req.pinfo_.cmdline_);
      LOG_INFO_STR(reconstructCommandLine(req));

      janosh_->setFormat(req.format_);

      if (!req.command_.empty()) {
        if (req.command_ == "trigger") {
          req.command_ = "set";
          req.runTriggers_ = true;
        }

        Tracker::setDoPublish(req.runTriggers_);
        JanoshThreadPtr dt(new DatabaseThread(janosh_,req, sso));
        dt->runSynchron();
        result = dt->result();

        sso << "__JANOSH_EOF\n" << std::to_string(result ? 0 : 1) << '\n';

        if (!result) {
          setResult(false);
        }
      } else {
        sso << "__JANOSH_EOF\n" << std::to_string(0) << '\n';
      }
      socket_.send(sso.str().c_str(), sso.str().size(), 0);
      setResult(true);
    } catch (std::exception& ex) {
      janosh::printException(ex);
      setResult(false);
      sso << "__JANOSH_EOF\n" << std::to_string(1) << '\n';
      if(transaction)
        janosh_->endTransaction(false);
      socket_.send(sso.str().c_str(), sso.str().size(), 0);
    }
  }
}
} /* namespace janosh */
