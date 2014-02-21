/*
 * exithandler.cpp
 *
 *  Created on: Feb 21, 2014
 *      Author: elchaschab
 */


#include <signal.h>
#include <boost/bind.hpp>

#include "logger.hpp"
#include "exithandler.hpp"

namespace janosh {

ExitHandler::ExitHandler(): ioservice_(), signals_(ioservice_, SIGINT, SIGTERM) {
  LOG_DEBUG("Registering exit handler");
  signals_.async_wait([&](const boost::system::error_code& error, int signal_number){this->runExitFuncs(error, signal_number);});
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

void ExitHandler::runExitFuncs(const boost::system::error_code& error, int signal_number) {
  std::cerr << "exit funcs" << std::endl;
  LOG_DEBUG_MSG("Running exit functions due to signal", signal_number);
  for(auto& e : exitFuncs_)
    e();
}

ExitHandler* ExitHandler::instance_ = NULL;
} /* namespace janosh */
