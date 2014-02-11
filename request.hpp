/*
 * request.hpp
 *
 *  Created on: Feb 11, 2014
 *      Author: elchaschab
 */

#ifndef REQUEST_HPP_
#define REQUEST_HPP_

#include <boost/serialization/vector.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <string>
#include <vector>
#include <iostream>
#include "format.hpp"

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
  vector<string> vecArgs_;
  vector<string> vecTargets_;
  bool runTriggers_ = false;
  bool verbose_ = false;

  Request() {
  }

  Request(Format& format, string& command, vector<string>& vecArgs, vector<string>& vecTargets, bool& triggers, bool& verbose) :
   format_(format),
   command_(command),
   vecArgs_(vecArgs),
   vecTargets_(vecTargets),
   runTriggers_(triggers),
   verbose_(verbose){
  }

  Request(const Request& other) {
    this->format_ = other.format_;
    this->command_ = other.command_;
    this->vecArgs_ = other.vecArgs_;
    this->vecTargets_ = other.vecTargets_;
    this->runTriggers_ = other.runTriggers_;
    this->verbose_ = other.verbose_;
  }
  virtual ~Request() {}

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
      ar & format_;
      ar & command_;
      ar & vecArgs_;
      ar & vecTargets_;
      ar & runTriggers_;
      ar & verbose_;
  }
};

void readRequest(Request& req, istream& is);
void writeRequest(Request& req, ostream& os);

} /* namespace janosh */

#endif /* REQUEST_HPP_ */
