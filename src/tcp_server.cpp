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


TcpServer::TcpServer(int maxThreads) : maxThreads_(maxThreads), threadLimit_(maxThreads) {
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
    TcpWorker* worker = new TcpWorker(threadLimit_,*client);
    worker->runAsynchron();
  }

  return true;
}
} /* namespace janosh */
