/*
 * CheesyServer.h
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#ifndef TCPSERVER_H_
#define TCPSERVER_H_

#include "format.hpp"
#include "semaphore.hpp"
#include <string>
#include <libsocket/unixserverstream.hpp>
#include <libsocket/exception.hpp>

namespace janosh {
namespace ls = libsocket;

class TcpServer {
  static TcpServer* instance_;
  int maxThreads_;
  ls::unix_stream_server clients_;

  TcpServer(int maxThreads);

public:
  Semaphore threadLimit_;

	virtual ~TcpServer();
	bool isOpen();
  void open(std::string url);
	void close();
	bool run();

	static TcpServer* getInstance(int maxThreads) {
	  if(instance_ == NULL)
	    instance_ = new TcpServer(maxThreads);
	  return instance_;
	}
};

} /* namespace janosh */

#endif /* CHEESYSERVER_H_ */
