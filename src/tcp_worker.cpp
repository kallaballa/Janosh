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
#include "compress.hpp"

namespace janosh {

TcpWorker::TcpWorker(Settings& settings, ls::unix_stream_client& socket) :
    JanoshThread("TcpWorker"),
    janosh_(new Janosh(settings)),
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


void TcpWorker::send(const string& msg) {
  uint64_t len = msg.size();
  socket_.snd((char*) &len, sizeof(len));
  socket_.snd(msg.c_str(), msg.size());
}


void TcpWorker::receive(string& msg) {
  uint64_t len;
  socket_.rcv((char*) &len, sizeof(len));
  msg.resize(len);
  socket_ >> msg;
}

void TcpWorker::run() {
  string request;
  Record::makeDB("127.0.0.1", 1978);
  while (true) {
    try {
      this->receive(request);
    } catch (std::exception& ex) {
      printException(ex);
      LOG_DEBUG_STR("End of request chain");
      setResult(false);
      break;
    }
    if(request.empty()) {
      break;
    }

    if(request == "begin") {
      LOG_DEBUG_STR("Transaction begin");
      janosh_->beginTransaction();
      send("bok");
      continue;
    } else if(request == "commit") {
      LOG_DEBUG_STR("Transaction commit");
      janosh_->endTransaction(true);
      send("cok");
      continue;
    } else if(request == "abort") {
      LOG_DEBUG_STR("Transaction about");
      janosh_->endTransaction(false);
      send("aok");
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
//      string compressed = compress_string(sso.str());
      this->send(sso.str());
      setResult(true);
    } catch (std::exception& ex) {
      janosh::printException(ex);
      setResult(false);
      sso << "__JANOSH_EOF\n" << std::to_string(1) << '\n';
      this->send(sso.str());
    }
  }
  Record::destroyDB();
}
} /* namespace janosh */
