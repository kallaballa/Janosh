#include <signal.h>

#include "logger.hpp"
#include "exithandler.hpp"
#include <signal.h>

namespace janosh {
ExitHandler* ExitHandler::instance_ = NULL;
static vector<ExitHandler::ExitFunc> exitFuncs_;

static void runExitFuncs(int signal_number) {
  for(auto& e : exitFuncs_)
    e();

  exit(1);
}

ExitHandler::ExitHandler() {
  LOG_DEBUG_STR("Registering exit handler");
  std::signal(SIGINT, [](int signal){ runExitFuncs(signal);});
  std::signal(SIGTERM, [](int signal){ runExitFuncs(signal);});
}

ExitHandler* ExitHandler::getInstance() {
  if(instance_ == NULL) {
    instance_ = new ExitHandler();
  }

  return instance_;
}

void ExitHandler::addExitFunc(ExitFunc e) {
  exitFuncs_.push_back(e);
}

} /* namespace janosh */
