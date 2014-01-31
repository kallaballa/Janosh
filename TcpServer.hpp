/*
 * CheesyServer.h
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#ifndef TCPSERVER_H_
#define TCPSERVER_H_


#include <string>
#include <vector>
#include <functional>
#include <boost/asio.hpp>
#include "format.hpp"

namespace janosh {
using std::function;
using std::string;
using std::vector;

class TcpServer {
    boost::asio::io_service io_service;
    boost::asio::ip::tcp::socket* socket;
    boost::asio::ip::tcp::acceptor acceptor;
public:
	TcpServer(int port);
	virtual ~TcpServer();
	void close();
	void run(function<int(Format,string,vector<string>, vector<string>, vector<string>, bool)> f);
};

} /* namespace janosh */

#endif /* CHEESYSERVER_H_ */
