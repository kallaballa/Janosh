#ifndef JANOSH_THREAD_HPP_
#define JANOSH_THREAD_HPP_

#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include "format.hpp"

namespace janosh {

using std::string;
using std::vector;
using std::ostream;
using std::thread;

class JanoshThread {
  Format format_;
  string command_;
  vector<string> vecArgs_;
  vector<string> vecTriggers_;
  vector<string> vecTargets_;
  bool verbose_;
  ostream& out_;
  std::thread* thread_;
public:
  JanoshThread(Format format, string command, vector<string> vecArgs, vector<string> vecTriggers, vector<string> vecTargets, bool verbose, ostream& out);
  ~JanoshThread();
  void join();
  int run();
};


} /* namespace janosh */

#endif /* JANOSH_THREAD_HPP_ */
