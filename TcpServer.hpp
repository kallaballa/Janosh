/*
 * CheesyServer.h
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#ifndef TCPSERVER_H_
#define TCPSERVER_H_

#include <boost/asio.hpp>
#include "format.hpp"

namespace janosh {

class TcpServer {
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::acceptor acceptor;
public:
	TcpServer(int port);
	virtual ~TcpServer();
	void close();
	void run();
};

} /* namespace janosh */

#endif /* CHEESYSERVER_H_ */
