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
  try {
    this->close();
  } catch(...) {
    //ignore
  }
}

void TcpClient::connect(string host, int port) {
  tcp::resolver resolver(io_service);
  tcp::resolver::query query(host, std::to_string(port));
  tcp::resolver::iterator iterator = resolver.resolve(query);
  boost::asio::connect(socket, iterator);
  socket.set_option(boost::asio::ip::tcp::no_delay(true));
}

bool endsWith(const std::string &mainStr, const std::string &toMatch)
{
  if(mainStr.size() >= toMatch.size() &&
      mainStr.compare(mainStr.size() - toMatch.size(), toMatch.size(), toMatch) == 0)
      return true;
    else
      return false;
}

int TcpClient::run(Request& req, std::ostream& out) {
  int returnCode = -1;
  try {
    boost::asio::streambuf request;
    std::ostream request_stream(&request);

    write_request(req, request_stream);

    boost::asio::write(socket, request);
    boost::asio::streambuf response;
    std::istream response_stream(&response);
    boost::array<char, 1> buf;

    socket.read_some(boost::asio::buffer(buf));

    returnCode = std::stoi(string() + buf[0]);

    if (returnCode == 0) {
      LOG_DEBUG_STR("Successful");
    } else {
      LOG_INFO_MSG("Failed", returnCode);
    }

    string line;
    while (response_stream) {
      boost::asio::read_until(socket, response, "\n");
      std::getline(response_stream, line);
      if(endsWith(line,"__JANOSH_EOF"))
        break;
      out << line << std::endl;
    }
  } catch (std::exception& ex) {
    LOG_ERR_MSG("Caught in tcp_client run", ex.what());
  }
  return returnCode;
}

void TcpClient::close() {
  LOG_DEBUG_MSG("Closing socket", &socket);
  socket.shutdown(boost::asio::socket_base::shutdown_both);
  socket.close();
}
} /* namespace janosh */
