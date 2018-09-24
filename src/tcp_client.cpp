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
    sock_(AF_SP, NN_PAIR) {
}

TcpClient::~TcpClient() {
  try {
    this->close();
  } catch(...) {
    //ignore
  }
}

void TcpClient::connect(string host, int port) {
  sock_.connect("ipc:///tmp/reqrep.ipc");
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
    char* buf = NULL;
    std::ostringstream request_stream;
    write_request(req, request_stream);

    sock_.send(request_stream.str().c_str(), request_stream.str().size(), 0);
    sock_.recv(&buf, NN_MSG, 0);
    std::istringstream response_stream(buf);

    string line;
    while (response_stream) {
      std::getline(response_stream, line);
      if(endsWith(line,"__JANOSH_EOF")) {
        std::getline(response_stream, line);
        if(line.empty())
          returnCode = 1;
        else
          returnCode = std::stoi(line);

        if (returnCode == 0) {
          LOG_DEBUG_STR("Successful");
        } else {
          LOG_INFO_MSG("Failed", returnCode);
        }
        //out << line.substr(0, line.size() - string("__JANOSH_EOF").size());

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
  sock_.shutdown(0);
  } catch(...) {
  }
}
} /* namespace janosh */
