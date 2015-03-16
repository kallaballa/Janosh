/*
 * CheesyClient.cpp
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#include <iostream>
#include "tcp_client.hpp"
#include "logger.hpp"

namespace janosh {

using boost::asio::ip::tcp;

TcpClient::TcpClient() :
    socket(io_service) {
}

TcpClient::~TcpClient() {
  this->close();
}

void TcpClient::connect(string host, int port) {
  tcp::resolver resolver(io_service);
  tcp::resolver::query query(host, std::to_string(port));
  tcp::resolver::iterator iterator = resolver.resolve(query);
  boost::asio::connect(socket, iterator);
  //socket.set_option(boost::asio::ip::tcp::no_delay(true));
}

int TcpClient::run(Request& req, std::ostream& out) {
  boost::asio::streambuf request;
  std::ostream request_stream(&request);

  write_request(req, request_stream);

  boost::asio::write(socket, request);
  boost::asio::streambuf response;
  std::istream response_stream(&response);
  boost::array<char, 1> buf;
  boost::array<char, 16> buf2;

  socket.read_some(boost::asio::buffer(buf));
  socket.read_some(boost::asio::buffer(buf2));

  revision_ = string(buf2.elems);
  int returnCode = std::stoi(string() + buf[0]);

  if (returnCode == 0) {
    LOG_DEBUG_STR("Successful");
  } else {
    LOG_INFO_MSG("Failed", returnCode);
  }

  try {
    string line;
    while (response_stream) {
      boost::asio::read_until(socket, response, "\n");
      std::getline(response_stream, line);
      if(line == "__JANOSH_EOF")
        break;
      out << line << std::endl;
    }
  } catch (std::exception& ex) {
  }
  return returnCode;
}

void TcpClient::close() {
  socket.close();
}

string TcpClient::revision() {
  return revision_;
}

} /* namespace janosh */
