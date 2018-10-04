/*
 * CheesyServer.h
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#ifndef TCPSERVER_H_
#define TCPSERVER_H_

#include "format.hpp"
#include <zmq.hpp>
#include <string>


namespace janosh {


class TcpServer {
  static TcpServer* instance_;
  int maxThreads_;
  zmq::context_t context_;
  zmq::socket_t clients_;
  zmq::socket_t workers_;

  TcpServer(int maxThreads);

public:
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
