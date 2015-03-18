/*
 * tracker.cpp
 *
 *  Created on: Feb 15, 2014
 *      Author: elchaschab
 */

#include "tracker.hpp"
#include "logger.hpp"


namespace janosh {
using std::cerr;
using std::endl;

map<thread::id, Tracker*> Tracker::instances_;
Tracker::Tracker() :
    printDirective_(DONTPRINT), revision_(0), context_(1), publisher_(context_, ZMQ_PUB) {
  const char * val = std::getenv("USER");

  if (val == NULL) {
    LOG_ERR_STR("Environment variable USER not found");
  } else {
    string url = (string("ipc:///tmp/janosh-") + string(val) + string(".ipc"));
    LOG_INFO_MSG("Binding ZMQ", url);
    publisher_.bind(url.c_str());
  }
}

Tracker::~Tracker() {
  publisher_.close();
  context_.close();
}

void Tracker::update(const string& s, const Operation& op) {
  map<string, size_t>& m = get(op);
  if(op == WRITE || op == DELETE) {
    ++revision_;
    string str = s + " " + s;
    zmq::message_t message(str.length());
    memcpy(message.data(), str.data(),str.length());
    publisher_.send(message);
  }
  auto iter = m.find(s);
  if(iter != m.end()) {
    m[s]++;
  } else {
    m[s]=1;
  }
}

size_t Tracker::get(const string& s, const Operation& op) {
  return get(op)[s];
}

map<string, size_t>& Tracker::get(const Operation& op) {
  switch(op) {
  case READ:
    return reads_;
    break;

  case WRITE:
    return writes_;
    break;

  case DELETE:
    return deletes_;
    break;

  case TRIGGER:
    return triggers_;
    break;
  }

  throw janosh_exception() << string_info({"Illegal operation map requested"});
}

void Tracker::reset() {
  print(cerr);

  reads_.clear();
  writes_.clear();
  deletes_.clear();
  triggers_.clear();
}

Tracker* Tracker::getInstancePerThread() {
  thread::id tid = std::this_thread::get_id();
  auto it = instances_.find(tid);
  Tracker* instance = NULL;
  if(it == instances_.end()) {
    instance = new Tracker();
    instances_[tid] = instance;
  } else {
    instance = (*it).second;
  }
  return instance;
}

void printHeader(ostream& out) {
  out << "--- Tracker Statistics ---" << endl;
}

void printFooter(ostream& out) {
  out << "--------------------------" << endl;
}

void Tracker::printMeta(ostream& out) {
  out << "r\t" << "w\t" << "d\t" << "t\t" << endl;
  out << reads_.size() << '\t' << writes_.size() << '\t' << deletes_.size() << '\t' << triggers_.size() << '\t' << endl;
}

void Tracker::printFull(ostream& out) {
  printMeta(out);
  if (!reads_.empty()) {
    printFooter(out);
    out << "Reads:" << endl;

    for (auto iter : reads_) {
      out << iter.first << ' ' << iter.second << endl;
    }
  }

  if (!writes_.empty()) {
    printFooter(out);
    out << "Writes:" << endl;

    for (auto iter : writes_) {
      out << iter.first << ' ' << iter.second << endl;
    }
  }

  if (!deletes_.empty()) {
    printFooter(out);
    out << "Deletes:" << endl;

    for (auto iter : deletes_) {
      out << iter.first << ' ' << iter.second << endl;
    }
  }

  if (!triggers_.empty()) {
    printFooter(out);
    out << "Triggers:" << endl;

    for (auto iter : triggers_) {
      out << iter.first << ' ' << iter.second << endl;
    }
  }
}

void Tracker::print(ostream& out) {
  PrintDirective pd = getPrintDirective();
  if(pd != DONTPRINT) {
    printHeader(out);
    if(getPrintDirective() == PRINTMETA)
      printMeta(cerr);
    else if(getPrintDirective() == PRINTFULL)
      printFull(cerr);
    printFooter(out);
  }
}

string Tracker::revision() {
  return std::to_string(revision_);
}

Tracker::PrintDirective Tracker::getPrintDirective() {
  return Tracker::getInstancePerThread()->printDirective_;
}

void Tracker::setPrintDirective(PrintDirective p) {
  Tracker::getInstancePerThread()->printDirective_ = p;
}

} /* namespace janosh */
