/*
 * CheesyServer.cpp
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#include <iostream>
#include <sstream>
#include <functional>
#include <thread>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <boost/bind.hpp>

#include "tcp_server.hpp"
#include "format.hpp"
#include "logger.hpp"
#include "tcp_worker.hpp"

#include "exception.hpp"
#include "exithandler.hpp"
#include "shared_pointers.hpp"
#include "janosh.hpp"


namespace janosh {

using std::string;
using std::vector;
using std::stringstream;
using std::function;
using std::ostream;

TcpServer* TcpServer::instance_;


TcpServer::TcpServer(int maxThreads) : maxThreads_(maxThreads), context_(1), clients_(context_, ZMQ_ROUTER), workers_(context_, ZMQ_DEALER) {
  ExitHandler::getInstance()->addExitFunc([&](){this->close();});
}

void TcpServer::open(int port) {
  clients_.bind("ipc:///tmp/janosh");
  workers_.bind ("inproc://workers");
}

TcpServer::~TcpServer() {
  this->close();
}


void TcpServer::close() {
  clients_.close();
  //sock_.shutdown(0);
}

bool TcpServer::run() {
  std::vector<TcpWorker*> workerVec;

  for(size_t i=0; i < maxThreads_; ++i) {
    workerVec.push_back(new TcpWorker(maxThreads_,&context_));
    workerVec[i]->runAsynchron();
  }
  try {
    zmq::proxy(clients_, workers_, NULL);
  } catch (janosh_exception& ex) {
    printException(ex);
    //shared->shutdown(0);
  } catch (std::exception& ex) {
    printException(ex);
    //shared->shutdown(0);
  } catch (...) {
    //shared->shutdown(0);
  }
  for(size_t i=0; i < maxThreads_; ++i) {
    delete workerVec[i];
  }

  return true;
}
} /* namespace janosh */
