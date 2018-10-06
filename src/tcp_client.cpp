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
    sock_(context_, ZMQ_DEALER) {
}

TcpClient::~TcpClient() {
}

std::string make_sessionkey() {
  std::mt19937 rng;
  rng.seed(std::random_device()());
  std::uniform_real_distribution<double> dist(0.0,1.0);

  string strRng = std::to_string(dist(rng));
  CryptoPP::SHA256 hash;
  byte digest[CryptoPP::SHA256::DIGESTSIZE];
  std::string output;

  hash.CalculateDigest(digest,(const byte *)strRng.c_str(),strRng.size());

  CryptoPP::HexEncoder encoder;
  CryptoPP::StringSink *SS = new CryptoPP::StringSink(output);
  encoder.Attach(SS);
  encoder.Put(digest,sizeof(digest));
  encoder.MessageEnd();

  return output;
}

void TcpClient::connect(string url) {
  try {
    sock_ = zmq::socket_t(context_, ZMQ_DEALER);
    string identity = make_sessionkey();
    sock_.setsockopt(ZMQ_IDENTITY, identity.data(), identity.size());
  } catch (...) {
    context_ = zmq::context_t(1);
    sock_ = zmq::socket_t(context_, ZMQ_DEALER);
    string identity = make_sessionkey();
    sock_.setsockopt(ZMQ_IDENTITY, identity.data(), identity.size());
  }
  sock_.connect(url.c_str());
  string begin="begin";
  zmq::message_t reply;
  sock_.send(begin.data(), begin.size());
  sock_.recv(&reply);
  string strReply;
  strReply.assign((const char*)reply.data(), reply.size());
  assert(strReply == "done");
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
    sock_.send(request_stream.str().c_str(), request_stream.str().size(), 0);

    zmq::message_t reply;
    sock_.recv(&reply);
    std::stringstream response_stream;
    response_stream.write((char*)reply.data(), reply.size());

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
    string message="commit";
    if(!commit)
      message="abort";
    zmq::message_t reply;
    sock_.send(message.data(), message.size());
    sock_.recv(&reply);
    string strReply;
    strReply.assign((const char*)reply.data(), reply.size());
    assert(strReply == "done");
    sock_.close();
}
} /* namespace janosh */
