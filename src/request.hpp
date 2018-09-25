/*
 * request.hpp
 *
 *  Created on: Feb 11, 2014
 *      Author: elchaschab
 */

#ifndef REQUEST_HPP_
#define REQUEST_HPP_

#include <boost/serialization/vector.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <string>
#include <vector>
#include <iostream>
#include "format.hpp"
#include "util.hpp"
#include "value.hpp"
namespace janosh {

using std::string;
using std::vector;
using std::istream;
using std::ostream;

class Request {
private:
  friend class boost::serialization::access;
public:
  Format format_ = janosh::Bash;
  string command_;
  vector<Value> vecArgs_;
  bool runTriggers_ = false;
  bool doTransaction_ = true;
  bool verbose_ = false;
  ProcessInfo pinfo_;
  string info_;

  Request() {
  }

  Request(const Format& format, const string& command, const vector<Value>& vecArgs, const bool& triggers, const bool& verbose, const bool& doTransaction, ProcessInfo pinfo_, const string& info) :
   format_(format),
   command_(command),
   vecArgs_(vecArgs),
   runTriggers_(triggers),
   doTransaction_(doTransaction),
   verbose_(verbose),
   pinfo_(pinfo_),
   info_() {
  }

  Request(const Request& other) {
    this->format_ = other.format_;
    this->command_ = other.command_;
    this->vecArgs_ = other.vecArgs_;
    this->runTriggers_ = other.runTriggers_;
    this->doTransaction_ = other.doTransaction_;
    this->verbose_ = other.verbose_;
    this->pinfo_ = other.pinfo_;
    this->info_ = other.info_;
  }
  virtual ~Request() {}

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
      ar & format_;
      ar & command_;
      ar & vecArgs_;
      ar & runTriggers_;
      ar & doTransaction_;
      ar & verbose_;
      ar & pinfo_;
      ar & info_;
  }
};

void read_request(Request& req, istream& is);
void write_request(Request& req, ostream& os);

} /* namespace janosh */

#endif /* REQUEST_HPP_ */
