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

using boost::asio::ip::tcp;
using std::string;
using std::vector;
using std::stringstream;
using std::function;
using std::ostream;

TcpServer* TcpServer::instance_;

TcpServer::TcpServer(int maxThreads) : io_service_(), acceptor_(io_service_), threadSema_(new Semaphore(maxThreads)){
  ExitHandler::getInstance()->addExitFunc([&](){this->close();});
}

void TcpServer::open(int port) {
  tcp::resolver resolver(io_service_);
  tcp::resolver::query query("0.0.0.0", std::to_string(port));
  boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);

  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen();
}

TcpServer::~TcpServer() {
  delete threadSema_;
  this->close();
}

bool TcpServer::isOpen() {
  return acceptor_.is_open();
}

void TcpServer::close() {
  io_service_.stop();
}

bool TcpServer::run() {
  boost::asio::ip::tcp::iostream* stream = new boost::asio::ip::tcp::iostream();
  stream->unsetf(std::ios_base::unitbuf);
  stream->tie(nullptr);

	try  {
	  acceptor_.accept(*stream->rdbuf());
	} catch(std::exception& ex) {
	  janosh::printException(ex);
	  stream->close();
	  return false;
	}

	threadSema_->wait();
	std::thread t([=]() {
	iostream_ptr shared(stream);
	try {

	  TcpWorker* w = NULL;
    try {
      do {
        if(w) {
          delete w;
          w = NULL;
        }
        w = new TcpWorker(shared);

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
    shared->close();
  } catch (std::exception& ex) {
    printException(ex);
    shared->close();
  } catch (...) {
    shared->close();
  }
  threadSema_->notify();
	});

	t.detach();

  
  return true;
}
} /* namespace janosh */
