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
  std::string dbhost_;
  int dbport_;
  TcpServer(int maxThreads, std::string dbhost, int dbport);

public:
	virtual ~TcpServer();
	bool isOpen();
  void open(std::string url);
	void close();
	bool run();

	static TcpServer* getInstance(int maxThreads, std::string dbhost, int dbport) {
	  if(instance_ == NULL)
	    instance_ = new TcpServer(maxThreads, dbhost, dbport);
	  return instance_;
	}
};

} /* namespace janosh */

#endif /* CHEESYSERVER_H_ */
