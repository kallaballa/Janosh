/*
 * janosh_thread.cpp
 *
 *  Created on: Feb 2, 2014
 *      Author: elchaschab
 */

#include "trigger_thread.hpp"
#include "commands.hpp"
#include "exception.hpp"
#include "tracker.hpp"

namespace janosh {

TriggerThread::TriggerThread(Request& req) : JanoshThread("Trigger"), req_(req), keysModified_(Tracker::getInstancePerThread()->get(Tracker::WRITE)) {
  LOG_DEBUG("Reset tracker");
  Tracker::getInstancePerThread()->reset();
}

void TriggerThread::run() {
  try {
    if(req_.runTriggers_ || !req_.vecTargets_.empty()) {
        if (req_.runTriggers_) {
          LOG_DEBUG("Triggers");
          Command* t = Janosh::getInstance()->cm_["trigger"];
          vector<string> triggers;
          for(auto iter : keysModified_) {
            triggers.push_back(iter.first);
          }

          (*t)(triggers, std::cerr);
        }

        if (!req_.vecTargets_.empty()) {
          LOG_DEBUG("Targets");
          Command* t = Janosh::getInstance()->cm_["target"];
          (*t)(req_.vecTargets_, std::cerr);
        }

        setResult(true);
        return;
    }
  } catch (janosh_exception& ex) {
    printException(ex);
  } catch (std::exception& ex) {
    printException(ex);
  }
}
} /* namespace janosh */
