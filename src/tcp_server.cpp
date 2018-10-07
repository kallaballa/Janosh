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


TcpServer::TcpServer(Settings& settings, int maxThreads) : maxThreads_(maxThreads), threadLimit_(maxThreads), settings_(settings) {
  ExitHandler::getInstance()->addExitFunc([&](){this->close();});
}

void TcpServer::open(string url) {
  clients_.setup(url.c_str());
}

TcpServer::~TcpServer() {
  this->close();
}


void TcpServer::close() {
  clients_.destroy();
}

bool TcpServer::run() {
  while(true) {
    threadLimit_.wait();
    ls::unix_stream_client* client;
    client = clients_.accept();

    std::thread t([=](){
      Logger::registerThread("TcpWorker");
      TcpWorker worker(settings_, *client);
      worker.runSynchron();
      threadLimit_.notify();
      Logger::removeThread();
    });

    t.detach();
  }

  return true;
}
} /* namespace janosh */
