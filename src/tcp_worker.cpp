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
std::mutex TcpWorker::tidMutex_;
std::string TcpWorker::ctid_;

static int workers = 0;
TcpWorker::TcpWorker(int maxThreads, zmq::context_t* context) :
    JanoshThread("TcpWorker"),
    janosh_(new Janosh()),
    threadSema_(new Semaphore(maxThreads)),
    context_(context),
    socket_(*context_, ZMQ_ROUTER) {
  socket_.connect("inproc://workers");
  string id = "workers" + std::to_string(workers++);
  socket_.setsockopt(ZMQ_IDENTITY, id.data(), id.size());

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

void TcpWorker::setCurrentTransactionID(const string& tid) {
  std::unique_lock<std::mutex>(tidMutex_);
  ctid_ = tid;
}

string TcpWorker::getCurrentTransactionID() {
  std::unique_lock<std::mutex>(tidMutex_);
  return ctid_;
}

void TcpWorker::process(string otherID, string id, string request, bool transaction) {
  string requestData;
  zmq::message_t sep;
   requestData.assign((const char*)request.data(), request.size());
   if(requestData == "begin") {
     LOG_DEBUG_MSG("Begin transaction", id);
     string reply = "done";
     socket_.send(otherID.data(), otherID.size(), ZMQ_SNDMORE);
     socket_.send(id.data(), id.size(), ZMQ_SNDMORE);
     socket_.send(sep, ZMQ_SNDMORE);
     socket_.send(reply.data(), reply.size());
     transaction = janosh_->beginTransaction();
     assert(transaction);
     setCurrentTransactionID(id);
     LOG_DEBUG_MSG("Begin end", id);
     return;
   } else if(requestData == "commit") {
     LOG_DEBUG_MSG("Commit transaction", id);

     string reply = "done";
     socket_.send(otherID.data(), otherID.size(), ZMQ_SNDMORE);
     socket_.send(id.data(), id.size(), ZMQ_SNDMORE);
     socket_.send(sep, ZMQ_SNDMORE);
     socket_.send(reply.data(), reply.size());
     janosh_->endTransaction(true);
     setCurrentTransactionID("");
     MessageQueue::Item i = {"","",""};
     while(true) {
       i = msgQueue_.pop();
       if(!i.id.empty())
         process(i.otherid, i.id, i.request, transaction);
       else
         break;
     }
     return;
   } else if(requestData == "abort") {
     LOG_DEBUG_MSG("Abort transaction", id);
     string reply = "done";
     socket_.send(otherID.data(), otherID.size(), ZMQ_SNDMORE);
     socket_.send(id.data(), id.size(), ZMQ_SNDMORE);
     socket_.send(sep, ZMQ_SNDMORE);
     socket_.send(reply.data(), reply.size());
     janosh_->endTransaction(false);
     setCurrentTransactionID("");
     MessageQueue::Item i = {"","",""};
     while(true) {
       i = msgQueue_.pop();
       if(!i.id.empty())
         process(i.otherid, i.id, i.request, transaction);
       else
         break;
     }
     return;
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
    socket_.send(otherID.data(), otherID.size(), ZMQ_SNDMORE);
    socket_.send(id.data(), id.size(), ZMQ_SNDMORE);
    socket_.send(sep.data(), sep.size(), ZMQ_SNDMORE);
    socket_.send(sso.str().c_str(), sso.str().size(), 0);
    setResult(true);
  } catch (std::exception& ex) {
    janosh::printException(ex);
    setResult(false);
    if(transaction)
      janosh_->endTransaction(false);
    sso << "__JANOSH_EOF\n" << std::to_string(1) << '\n';
    socket_.send(otherID.data(), otherID.size(), ZMQ_SNDMORE);
    socket_.send(id.data(), id.size(), ZMQ_SNDMORE);
    socket_.send(sep.data(), sep.size(), ZMQ_SNDMORE);
    socket_.send(sso.str().c_str(), sso.str().size(), 0);
  }
}
void TcpWorker::run() {
  zmq::message_t identity;
  zmq::message_t otherID;
  zmq::message_t sep;
  zmq::message_t request;
  zmq::message_t copied_id;
  zmq::message_t copied_otherId;
  zmq::message_t transaction_id;
  bool transaction = false;
  string strOtherID;
  string strIdentity;
  string strRequest;

  string strSep;
  while (true) {
    try {
      LOG_DEBUG_STR("RECEIVE START");
      socket_.recv(&otherID);
      copied_otherId.copy(&otherID);
      strOtherID.assign((const char*)otherID.data(), otherID.size());
      std::cerr << strOtherID << std::endl;

      socket_.recv(&identity);
      copied_id.copy(&identity);
      strIdentity.assign((const char*)identity.data(), identity.size());
      std::cerr << strIdentity << std::endl;

      socket_.recv(&sep);

      socket_.recv(&request);
      strRequest.assign((const char*)request.data(), request.size());
      std::cerr << strRequest << std::endl;
      LOG_DEBUG_STR("RECEIVE END");
      if(!getCurrentTransactionID().empty() && strIdentity != getCurrentTransactionID()) {
        LOG_DEBUG_STR("PUSH");
        msgQueue_.push(strOtherID, strIdentity, strRequest);
        continue;
      }

    } catch (std::exception& ex) {
      printException(ex);
      LOG_DEBUG_STR("End of request chain");
      setResult(false);
      return;
    }


    process(strOtherID, strIdentity, strRequest, transaction);
  }
}
} /* namespace janosh */
