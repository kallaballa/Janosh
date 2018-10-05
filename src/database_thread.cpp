/*
 * janosh_thread.cpp
 *
 *  Created on: Feb 2, 2014
 *      Author: elchaschab
 */

#include "database_thread.hpp"
#include "commands.hpp"
#include "exception.hpp"

namespace janosh {

DatabaseThread::DatabaseThread(std::shared_ptr<Janosh> janosh, const Request& req, std::ostream& out) :
    JanoshThread("Database"),
    janosh_(janosh),
    req_(req),
    out_(out) {
}

void DatabaseThread::run() {

  try {
    setResult(false);


    if (!req_.command_.empty()) {
      LOG_DEBUG_MSG("Execute command", req_.command_);
      Command* cmd = janosh_->cm_[req_.command_];

      if (!cmd) {
        throw janosh_exception() << string_info( { "Unknown command", req_.command_ });
      }

      Command::Result r;
      r = (*cmd)(req_.vecArgs_, out_);
      if (r.first == -1)
        throw janosh_exception() << msg_info(r.second);
      setResult(true);

      LOG_INFO_MSG(r.second, r.first);
    } else {
      throw janosh_exception() << msg_info("missing command");
    }
    if(req_.doTransaction_)
      janosh_->endTransaction(true);
  } catch (janosh_exception& ex) {
    if(req_.doTransaction_)
      janosh_->endTransaction(false);
    printException(ex);
    return;
  } catch (std::exception& ex) {
    if(req_.doTransaction_)
      janosh_->endTransaction(false);
    printException(ex);
    return;
  }
}
} /* namespace janosh */
