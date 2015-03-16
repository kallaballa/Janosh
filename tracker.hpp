/*
 * tracker.hpp
 *
 *  Created on: Feb 15, 2014
 *      Author: elchaschab
 */

#ifndef TRACKER_HPP_
#define TRACKER_HPP_

#include <string>
#include <map>
#include "path.hpp"
#include "exception.hpp"
#include <iostream>
#include <thread>
#include "cppzmq/zmq.hpp"

namespace janosh {
using std::string;
using std::map;
using std::ostream;
using std::thread;
class Tracker {
public:
  enum PrintDirective {
    DONTPRINT,
    PRINTMETA,
    PRINTFULL
  };
private:
  map<string, size_t> reads_;
  map<string, size_t> writes_;
  map<string, size_t> deletes_;
  map<string, size_t> triggers_;
  static map<thread::id, Tracker*> instances_;
  PrintDirective printDirective_;
  void printMeta(ostream& out);
  void printFull(ostream& out);
  long long revision_;
  zmq::context_t context_;
  zmq::socket_t publisher_;
public:
  enum Operation {
    READ,
    WRITE,
    DELETE,
    TRIGGER
  };

  Tracker() : printDirective_(DONTPRINT), revision_(0), context_(1), publisher_(context_, ZMQ_PUB) {
    const char * val = std::getenv( "USER" );

    if(val == NULL) {
      LOG_ERR_STR("Environment variable USER not found");
    } else {
      publisher_.bind((string("ipc://janosh-") + string(val) + string(".ipc")).c_str());
    }
  };

  virtual ~Tracker() {};

  void update(const string& s, const Operation& op);
  size_t get(const string& s, const Operation& op);
  map<string, size_t>& get(const Operation& op);
  void reset();
  void print(ostream& out);
  string revision();

  static Tracker* getInstancePerThread();
  static void setPrintDirective(PrintDirective p);
  static PrintDirective getPrintDirective();
};

} /* namespace janosh */

#endif /* TRACKER_HPP_ */
