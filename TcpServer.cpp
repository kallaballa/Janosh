/*
 * CheesyServer.cpp
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#include <iostream>
#include <sstream>
#include <functional>
#include <thread>
#include <assert.h>
#include "TcpServer.hpp"
#include "format.hpp"
#include "logger.hpp"
#include "janosh_thread.hpp"

namespace janosh {

using boost::asio::ip::tcp;
using std::string;
using std::vector;
using std::stringstream;
using std::function;
using std::ostream;

TcpServer::TcpServer(int port) : io_service(), acceptor(io_service){
    tcp::resolver resolver(io_service);
    tcp::resolver::query query("0.0.0.0", std::to_string(port));
    boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);

    acceptor.open(endpoint.protocol());
    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor.bind(endpoint);
}

TcpServer::~TcpServer() {

}

void splitAndPushBack(string& s, vector<string>& vec) {
  std::size_t i;
  std::size_t lasti = 0;
  string arg;

  while((i = s.find(",", lasti)) != string::npos) {
    arg = s.substr(lasti, i);
    if(!arg.empty())
      vec.push_back(arg);
    lasti = i + 1;
  }

  arg = s.substr(lasti, s.size());
  if(!arg.empty())
    vec.push_back(arg);
}

void TcpServer::run() {
	acceptor.listen();
	boost::asio::ip::tcp::socket* socket = new boost::asio::ip::tcp::socket(io_service);
	acceptor.accept(*socket);

	try {
    Format format;
    string command;
    vector<string> vecArgs;
    vector<string> vecTriggers;
    vector<string> vecTargets;
    bool verbose;

    std::string peerAddr = socket->remote_endpoint().address().to_string();

//    LOG_DEBUG_MSG("accepted", peerAddr);
    string line;
    boost::asio::streambuf response;
    boost::asio::read_until(*socket, response, "\n");
    std::istream response_stream(&response);

    std::getline(response_stream, line);

 //   LOG_DEBUG_MSG("format", line);
    if (line == "BASH") {
      format = janosh::Bash;
    } else if (line == "JSON") {
      format = janosh::Json;
    } else if (line == "RAW") {
      format = janosh::Raw;
    } /*else
      throw janosh_exception() << string_info( { "Illegal formats line", line });*/

    std::getline(response_stream, command);
  //  LOG_DEBUG_MSG("command", command);

    std::getline(response_stream, line);
  //  LOG_DEBUG_MSG("args", line);

    splitAndPushBack(line, vecArgs);

    std::getline(response_stream, line);
   // LOG_DEBUG_MSG("triggers", line);

    splitAndPushBack(line, vecTriggers);

    std::getline(response_stream, line);
  //  LOG_DEBUG_MSG("targets", line);

    splitAndPushBack(line, vecTargets);

    std::getline(response_stream, line);
  //  LOG_DEBUG_MSG("verbose", line);

    if (line == "TRUE")
      verbose = true;
    else if (line == "FALSE")
      verbose = false;
/*    else
      throw janosh_exception() << string_info( { "Illegal verbose line", line });*/

    boost::asio::streambuf* out_buf = new boost::asio::streambuf();
    ostream* out_stream = new ostream(out_buf);
    std::cout.rdbuf(out_stream->rdbuf());

    JanoshThread* jt = new JanoshThread(format, command, vecArgs, vecTriggers, vecTargets, verbose, *out_stream);

    int rc = jt->run();

    boost::asio::streambuf rc_buf;
    ostream rc_stream(&rc_buf);
    rc_stream << std::to_string(rc) << '\n';
 //   LOG_DEBUG_MSG("sending", rc_buf.size());
    boost::asio::write(*socket, rc_buf);

    std::thread flusher([=]{
      jt->join();
//      LOG_DEBUG_MSG("sending", out_buf->size());
      boost::asio::write(*socket, *out_buf);
      socket->close();
      delete out_buf;
      delete out_stream;
      delete jt;
    });

    flusher.detach();
  } catch (std::exception& ex) {
  }
}
} /* namespace janosh */
