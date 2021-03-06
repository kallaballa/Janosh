/*
 * settings.h
 *
 *  Created on: Feb 9, 2014
 *      Author: elchaschab
 */

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <boost/filesystem.hpp>
#include <vector>
#include <string>
#include "json_spirit/json_spirit.h"
#include "logger.hpp"

namespace janosh {
namespace fs = boost::filesystem;
namespace js = json_spirit;

using std::vector;
using std::string;

class Settings {
public:
  fs::path janoshFile;
  int maxThreads;
  string dbString;
  string ktopts;
  string bindUrl;
  string connectUrl;

  Settings();
  template<typename T> void error(const string& msg, T t, int exitcode=1) {
    std::cerr << "Settings broken:" << msg << std::endl;
    exit(exitcode);
  }
private:
  bool find(const js::Object& obj, const string& name, js::Value& value);
};

} /* namespace janosh */

#endif /* SETTINGS_H_ */
