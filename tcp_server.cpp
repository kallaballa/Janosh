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
#include "janosh_thread.hpp"
#include "exception.hpp"
#include "cache.hpp"
#include "exithandler.hpp"

namespace janosh {

using boost::asio::ip::tcp;
using std::string;
using std::vector;
using std::stringstream;
using std::function;
using std::ostream;

TcpServer* TcpServer::instance_;

TcpServer::TcpServer() : io_service_(), acceptor_(io_service_), cache_() {
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
  this->close();
}

bool TcpServer::isOpen() {
  return acceptor_.is_open();
}

void TcpServer::close() {
  io_service_.stop();
}

string reconstructCommandLine(Request& req) {
  string cmdline = "janosh ";

  if(req.verbose_)
    cmdline += "-v ";

  if(req.format_ == janosh::Bash)
    cmdline += "-b ";
  else if(req.format_ == janosh::Json)
    cmdline += "-j ";
  else if(req.format_ == janosh::Raw)
    cmdline += "-r ";

  if(req.runTriggers_)
     cmdline += "-t ";

   if(!req.vecTargets_.empty()) {
     cmdline += "-e ";
     bool first = true;
     for(const string& target : req.vecTargets_) {
       if(!first)
         cmdline+=",";
       cmdline+=target;

       first = false;
     }
     cmdline += " ";
   }

   cmdline += (req.command_ + " ");

   if(!req.vecArgs_.empty()) {
     bool first = true;
     for(const string& arg : req.vecArgs_) {
       if(!first)
         cmdline+=" ";
       cmdline+= ("\"" + arg + "\"");

       first = false;
     }
     cmdline += " ";
   }

  return cmdline;
}

bool TcpServer::run() {
	boost::asio::ip::tcp::socket* socket = NULL;

	try  {
	  socket = new boost::asio::ip::tcp::socket(io_service_);
	  acceptor_.accept(*socket);
	} catch(std::exception& ex) {
    if (socket != NULL) {
      LOG_DEBUG_MSG("Closing socket", socket);
      socket->shutdown(boost::asio::socket_base::shutdown_both);
      socket->close();
      delete socket;
    }
	  janosh::printException(ex);
	  return false;
	}

	try {
    Request req;
    bool cacheable=false;
    bool cachehit=false;
	  try {
      std::string peerAddr = socket->remote_endpoint().address().to_string();
      LOG_DEBUG_MSG("accepted", peerAddr);
      LOG_DEBUG_MSG("socket", socket);

      boost::asio::streambuf response;
      boost::asio::read_until(*socket, response, "\n");
      std::istream response_stream(&response);

      read_request(req, response_stream);

      LOG_DEBUG_MSG("ppid", req.pinfo_.pid_);
      LOG_DEBUG_MSG("cmdline", req.pinfo_.cmdline_);
      LOG_INFO_STR(reconstructCommandLine(req));

      // only "-j get /." is cached
      cacheable = req.command_ == "get"
          && req.format_ == janosh::Json
          && !req.runTriggers_
          && req.vecTargets_.empty()
          && req.vecArgs_.size() == 1
          && req.vecArgs_[0] == "/.";

      if(!cacheable && (!req.command_.empty() && (req.command_ != "get" && req.command_ != "dump" && req.command_ != "hash" && req.command_ != "size"))) {
        LOG_DEBUG_STR("Invalidating cache");
        cache_.invalidate();
      }

      cachehit = cacheable && cache_.isValid();

      LOG_DEBUG_MSG("cacheable", cacheable);
      LOG_DEBUG_MSG("cachehit", cachehit);
	  } catch (std::exception& ex) {
      try {
        LOG_DEBUG_MSG("Closing socket", socket);
        socket->shutdown(boost::asio::socket_base::shutdown_both);
        socket->close();
        delete socket;
      } catch (std::exception& ex) {
        janosh::printException(ex);
      }
      return true;
	  }

    if(!cachehit) {
      boost::asio::streambuf* out_buf = NULL;
      ostream* out_stream = NULL;
      JanoshThread* jt = NULL;

      try {
        out_buf = new boost::asio::streambuf();
        out_stream = new ostream(out_buf);
        jt = new JanoshThread(req, *out_stream);

        int rc = jt->run();
        boost::asio::streambuf rc_buf;
        ostream rc_stream(&rc_buf);
        rc_stream << std::to_string(rc) << '\n';
        LOG_DEBUG_MSG("sending", rc_buf.size());
        boost::asio::write(*socket, rc_buf);
      } catch (std::exception& ex) {
        try {
          LOG_DEBUG_MSG("Closing socket", socket);
          socket->shutdown(boost::asio::socket_base::shutdown_both);
          socket->close();
          delete socket;
        } catch (std::exception& ex) {
          janosh::printException(ex);
        }

        if(out_buf)
          delete out_buf;
        if(out_stream)
          delete out_stream;
        if(jt)
          delete jt;

        return true;
      }

      std::thread flusher([=]{
        Logger::registerThread("Flusher");
        try {
          jt->join();
          LOG_DEBUG_MSG("sending", out_buf->size());
          if(cacheable) {
            LOG_DEBUG_STR("updating cache");
            cache_.setData(boost::asio::buffer_cast<const char*>(out_buf->data()), out_buf->size());
          }
          boost::asio::write(*socket, *out_buf);
        } catch(std::exception& ex) {
          janosh::printException(ex);
        }
        try {
          LOG_DEBUG_MSG("Closing socket", socket);
          socket->shutdown(boost::asio::socket_base::shutdown_both);
          socket->close();
          delete socket;
        } catch (std::exception& ex) {
          janosh::printException(ex);
        }
        if(out_buf)
          delete out_buf;
        if(out_stream)
          delete out_stream;
        if(jt)
          delete jt;
        Logger::removeThread();
      });

      flusher.detach();
    } else {
      std::thread cacheWriter([=]() {
        Logger::registerThread("CacheWriter");
        cache_.lock();
        try {
          boost::asio::streambuf rc_buf;
          ostream rc_stream(&rc_buf);
          rc_stream << std::to_string(0) << '\n';
          LOG_DEBUG_MSG("sending", rc_buf.size());
          boost::asio::write(*socket, rc_buf);
          LOG_DEBUG_MSG("sending", cache_.getSize());
          boost::asio::write(*socket, boost::asio::buffer(cache_.getData(), cache_.getSize()));
        } catch(std::exception& ex) {
          janosh::printException(ex);
        }
        cache_.unlock();
        try {
          LOG_DEBUG_MSG("Closing socket", socket);
          socket->shutdown(boost::asio::socket_base::shutdown_both);
          socket->close();
          delete socket;
        } catch (std::exception& ex) {
          janosh::printException(ex);
        }
        Logger::removeThread();
      });

      cacheWriter.detach();
    }
  } catch (janosh_exception& ex) {
    printException(ex);
  } catch (std::exception& ex) {
    printException(ex);
  }
  return true;
}
} /* namespace janosh */
