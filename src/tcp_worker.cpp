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
    socket_(*context_, ZMQ_DEALER) {
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
  zmq::message_t identity;
  zmq::message_t request;
  zmq::message_t copied_id;
  zmq::message_t copied_request;
  zmq::message_t transaction_id;
  bool transaction = false;
  while (true) {
    try {
      socket_.recv(&identity);
      socket_.recv(&request);
    } catch (std::exception& ex) {
      printException(ex);
      LOG_DEBUG_STR("End of request chain");
      setResult(false);
      return;
    }
    string strIdentity;
    strIdentity.assign((const char*)identity.data(), identity.size());
    string requestData;
    requestData.assign((const char*)request.data(), request.size());
    if(requestData == "begin") {
      LOG_DEBUG_MSG("Begin transaction", strIdentity);
      string reply = "done";
      copied_id.copy(&identity);
      transaction_id.copy(&identity);
      socket_.send(copied_id, ZMQ_SNDMORE);
      socket_.send(reply.data(), reply.size());
      transaction = janosh_->beginTransaction();
      assert(transaction);
      LOG_DEBUG_MSG("Begin end", strIdentity);
      continue;
    } else if(requestData == "commit") {
      LOG_DEBUG_MSG("Commit transaction", strIdentity);
      string strTransaction;
      strTransaction.assign((const char*)transaction_id.data(), transaction_id.size());
      assert(strIdentity == strTransaction);

      string reply = "done";
      copied_id.copy(&identity);
      socket_.send(copied_id, ZMQ_SNDMORE);
      socket_.send(reply.data(), reply.size());
      assert(transaction);
      janosh_->endTransaction(true);
      continue;
    } else if(requestData == "abort") {
      LOG_DEBUG_MSG("Abort transaction", strIdentity);
      string strTransaction;
      strTransaction.assign((const char*)transaction_id.data(), transaction_id.size());
      assert(strIdentity == strTransaction);

      string reply = "done";
      copied_id.copy(&identity);
      socket_.send(copied_id, ZMQ_SNDMORE);
      socket_.send(reply.data(), reply.size());
      assert(transaction);
      janosh_->endTransaction(false);
      continue;
    }

    std::ostringstream sso;
    bool result = false;
    try {
      Request req;
      std::stringstream response_stream;
      response_stream.write((char*) request.data(), request.size());
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
      copied_id.copy(&identity);
      socket_.send(copied_id, ZMQ_SNDMORE);
      socket_.send(sso.str().c_str(), sso.str().size(), 0);
      setResult(true);
    } catch (std::exception& ex) {
      janosh::printException(ex);
      setResult(false);
      if(transaction)
        janosh_->endTransaction(false);
      sso << "__JANOSH_EOF\n" << std::to_string(1) << '\n';
      copied_id.copy(&identity);
      socket_.send(copied_id, ZMQ_SNDMORE);
      socket_.send(sso.str().c_str(), sso.str().size(), 0);
    }
  }
}
} /* namespace janosh */
