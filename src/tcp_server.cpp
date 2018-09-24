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

TcpServer::TcpServer(int maxThreads) : maxThreads_(maxThreads), context_(1), sock_(context_, ZMQ_REP) {
  ExitHandler::getInstance()->addExitFunc([&](){this->close();});
}

void TcpServer::open(int port) {
  sock_.bind("tcp://*:" + std::to_string(port));
}

TcpServer::~TcpServer() {
  this->close();
}


void TcpServer::close() {
  //sock_.shutdown(0);
}

bool TcpServer::run() {
	socket_ptr shared(&sock_);
	try {

	  TcpWorker* w = NULL;
    try {
      do {
        if(w) {
          delete w;
          w = NULL;
        }
        w = new TcpWorker(maxThreads_, shared);

        w->runSynchron();
      } while(w->connected());
    } catch (...) {
      if(w) {
        delete w;
        w = NULL;
      }
      throw;
    }
    if(w) {
      delete w;
      w = NULL;
    }

  } catch (janosh_exception& ex) {
    printException(ex);
    //shared->shutdown(0);
  } catch (std::exception& ex) {
    printException(ex);
    //shared->shutdown(0);
  } catch (...) {
    //shared->shutdown(0);
  }
//  threadSema_->notify();
//	});

//	t.detach();

  
  return true;
}
} /* namespace janosh */
