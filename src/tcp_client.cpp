/*
 * CheesyClient.cpp
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#include <iostream>
#include "tcp_client.hpp"
#include "logger.hpp"
#include "compress.hpp"


namespace janosh {

TcpClient::TcpClient() {
}

TcpClient::~TcpClient() {
}

void TcpClient::connect(string url) {
  sock_.connect(url.c_str());
  send("begin");
  string msg;
  receive(msg);
  assert(msg == "bok");
}

void TcpClient::send(const string& msg) {
  uint64_t len = msg.size();
  sock_.snd((char*) &len, sizeof(len));
  sock_.snd(msg.c_str(), msg.size());
}


void TcpClient::receive(string& msg) {
  uint64_t len;
  sock_.rcv((char*) &len, sizeof(len));
  msg.resize(len);
  sock_ >> msg;
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
    std::ostringstream request_stream;
    write_request(req, request_stream);
    this->send(request_stream.str());
    this->receive(rcvBuffer_);

    std::stringstream response_stream;
    response_stream.write((char*)rcvBuffer_.data(), rcvBuffer_.size());
    string line;
    while (response_stream) {
      std::getline(response_stream, line);
      if(endsWith(line,"__JANOSH_EOF")) {
        out << line.substr(0, line.size() - string("__JANOSH_EOF").size());
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

void TcpClient::close(bool commit) {
    LOG_DEBUG_STR("Closing socket");
    if(commit)
      send("commit");
    else
      send("abort");
    string msg;
    receive(msg);
    if(commit)
      assert(msg == "cok");
    else
      assert(msg == "aok");
    sock_.destroy();
}
} /* namespace janosh */
