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

TcpClient::TcpClient() :
    context_(1),
    sock_(NULL) {
}

TcpClient::~TcpClient() {
  try {
    this->close();
  } catch(...) {
    //ignore
  }
  delete sock_;
}

void TcpClient::connect(string host, int port) {
  if(sock_ != NULL)
    delete sock_;
  sock_ = new zmq::socket_t(context_, ZMQ_REQ);
  sock_->setsockopt(ZMQ_IMMEDIATE, 1);
  sock_->setsockopt(ZMQ_RECONNECT_IVL, -1);
  sock_->connect(("tcp://" + host + ":" + std::to_string(port)).c_str());
  std::cerr << sock_->connected() << std::endl;
}

bool endsWith(const std::string &mainStr, const std::string &toMatch)
{
  if(mainStr.size() >= toMatch.size() &&
      mainStr.compare(mainStr.size() - toMatch.size(), toMatch.size(), toMatch) == 0)
      return true;
    else
      return false;
}

std::string string_to_hex(const std::string& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();

    std::string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
        output.push_back(' ');
    }
    return output;
}


int TcpClient::run(Request& req, std::ostream& out) {
  int returnCode = -1;
  try {
    std::ostringstream request_stream;
    write_request(req, request_stream);
    sock_->send(request_stream.str().c_str(), request_stream.str().size(), 0);
    zmq::message_t reply;
    sock_->recv(&reply);
    std::stringstream response_stream;
    response_stream.write((char*)reply.data(), reply.size());
    string line;
    while (response_stream) {
      std::getline(response_stream, line);
      if(endsWith(line,"__JANOSH_EOF")) {
        out << line.substr(0, line.size() - string("__JANOSH_EOF").size()) << '\n';
        std::getline(response_stream, line);
        returnCode = std::stoi(line);

        if (returnCode == 0) {
          LOG_DEBUG_STR("Successful");
        } else {
          LOG_INFO_MSG("Failed", returnCode);
        }

        break;
      }
      out << line << '\n';
    }
  } catch (std::exception& ex) {
    LOG_ERR_MSG("Caught in tcp_client run", ex.what());
  }
  return returnCode;
}

void TcpClient::close() {
  try {
    LOG_DEBUG_STR("Closing socket");
    sock_->close();
  } catch(...) {
  }
}
} /* namespace janosh */
