/*
 * janosh_thread.cpp
 *
 *  Created on: Feb 2, 2014
 *      Author: elchaschab
 */

#include "database_thread.hpp"
#include "janosh.hpp"
#include "commands.hpp"
#include "exception.hpp"
#include "trigger_thread.hpp"

namespace janosh {

DatabaseThread::DatabaseThread(const Request& req, socket_ptr socket, ostream_ptr out) :
    JanoshThread("Database"),
    req_(req),
    socket_(socket),
    out_(out) {
}

void DatabaseThread::run() {
  int rc = 1;
  try {
    Janosh* instance = Janosh::getInstance();
    instance->setFormat(req_.format_);

    if (!req_.command_.empty()) {
      LOG_DEBUG_MSG("Execute command", req_.command_);
      Command* cmd = instance->cm_[req_.command_];

      if (!cmd) {
        throw janosh_exception() << string_info( { "Unknown command", req_.command_ });
      }

      Command::Result r = (*cmd)(req_.vecArgs_, *out_);
      if (r.first == -1)
        throw janosh_exception() << msg_info(r.second);

      LOG_INFO_MSG(r.second, r.first);
    } else if (req_.vecTargets_.empty()) {
      throw janosh_exception() << msg_info("missing command");
    }
    rc = 0;

    TriggerThread* triggerThread = new TriggerThread(req_, out_);
    triggerThread->runSynchron();
  } catch (janosh_exception& ex) {
    printException(ex);
    return;
  } catch (std::exception& ex) {
    printException(ex);
    return;
  }

  try {
    boost::asio::streambuf rc_buf;
    ostream rc_stream(&rc_buf);
    rc_stream << std::to_string(rc) << '\n';
    LOG_DEBUG_MSG("sending", rc_buf.size());
    boost::asio::write(*socket_, rc_buf);
    setResult(true);
  } catch (janosh_exception& ex) {
    printException(ex);
    return;
  } catch (std::exception& ex) {
    printException(ex);
    return;
  }
}
} /* namespace janosh */
