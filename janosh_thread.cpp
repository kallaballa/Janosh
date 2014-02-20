/*
 * janosh_thread.cpp
 *
 *  Created on: Feb 2, 2014
 *      Author: elchaschab
 */

#include "janosh_thread.hpp"
#include "janosh.hpp"
#include "commands.hpp"
#include <thread>
#include "exception.hpp"
#include "tracker.hpp"

namespace janosh {

JanoshThread::JanoshThread(Request& req, ostream& out) :
    req_(req),
    out_(out),
    thread_(NULL) {
}

JanoshThread::~JanoshThread() {
  if(thread_ != NULL)
    delete thread_;
}

void JanoshThread::join() {
  if(thread_ != NULL)
    thread_->join();
}

int JanoshThread::run() {
  try {
    Janosh* instance = Janosh::getInstance();
    Tracker* tracker = Tracker::getInstance();
    instance->setFormat(req_.format_);

    if (!req_.command_.empty()) {
      LOG_DEBUG_MSG("Execute command", req_.command_);
      Command* cmd = instance->cm_[req_.command_];

      if (!cmd) {
        throw janosh_exception() << string_info( { "Unknown command", req_.command_ });
      }

      Command::Result r = (*cmd)(req_.vecArgs_, out_);
      if (r.first == -1)
        throw janosh_exception() << msg_info(r.second);

      LOG_INFO_MSG(r.second, r.first);
    } else if (req_.vecTargets_.empty()) {
      throw janosh_exception() << msg_info("missing command");
    }

    assert(thread_ == NULL);
    if(req_.runTriggers_ || !req_.vecTargets_.empty()) {
      thread_ = new std::thread([=](){
        Logger::registerThread("Trigger");
        if (req_.runTriggers_) {
          Command* t = Janosh::getInstance()->cm_["trigger"];
          map<string, size_t>& keysModified = tracker->get(Tracker::WRITE);

          vector<string> triggers;
          for(auto iter : keysModified) {
            triggers.push_back(iter.first);
          }

          (*t)(triggers, out_);
        }
        LOG_DEBUG("Reset tracker");
        tracker->reset();
        if (!req_.vecTargets_.empty()) {
          LOG_DEBUG("Targets");
          Command* t = Janosh::getInstance()->cm_["target"];
          (*t)(req_.vecTargets_, out_);
        }
        Logger::removeThread();
      });
    }
  } catch (janosh_exception& ex) {
    printException(ex);
    return 1;
  } catch (std::exception& ex) {
    printException(ex);
    return 1;
  }

  return 0;
}
} /* namespace janosh */
