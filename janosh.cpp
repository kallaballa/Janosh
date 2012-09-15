#include "janosh.hpp"
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>

using namespace std;

using std::cerr;
using std::cout;
using std::endl;
using std::map;
using std::set;
using std::vector;
using std::ifstream;
using std::pair;
using std::string;

namespace fs = boost::filesystem;

namespace janosh {
  class Settings {
  public:
    fs::path janoshFile;
    fs::path databaseFile;
    fs::path triggerFile;
    vector<fs::path> triggerDirs;

    Settings() {
      const char* home = getenv ("HOME");
      if (home==NULL)
        error("Can't find environment variable.", "HOME");

      string janoshDir = string(home) + "/.janosh/";
      fs::path dir(janoshDir);

      if (!fs::exists(dir)) {
        if (!fs::create_directory(dir)) {
          error("can't create directory", dir.string());
        }
      }

      this->janoshFile = fs::path(dir.string() + "janosh.json");
      this->triggerFile = fs::path(dir.string() + "triggers.json");

      if(!fs::exists(janoshFile)) {
        error("janosh configuration not found: ", janoshFile);
      } else {
        js::Value janoshConf;

        try{
          ifstream is(janoshFile.c_str());
          js::read(is,janoshConf);
          js::Object jObj = janoshConf.get_obj();
          js::Value v;

          if(find(jObj, "triggerDirectories", v)) {
            BOOST_FOREACH (const js::Value& vDir, v.get_array()) {
              triggerDirs.push_back(fs::path(vDir.get_str()));
            }
          }

          if(find(jObj, "database", v)) {
            this->databaseFile = fs::path(v.get_str());
          } else {
            error(this->janoshFile.string(), "No database file defined");
          }
        } catch (exception& e) {
          error("Unable to load jashon configuration", e.what());
        }
      }
    }
  private:
    bool find(const js::Object& obj, const string& name, js::Value& value) {
      auto it = find_if(obj.begin(), obj.end(),
          [&](const js::Pair& p){ return p.name_ == name;});
      if (it != obj.end()) {
        value = (*it).value_;
        return true;
      }
      return false;
    }
  };
}

void printUsage() {
  std::cerr << "janosh [-l] <path>" << endl
      << endl
      << "<path>         the json path (uses / as separator)" << endl
      << endl
      << "Options:" << endl
      << endl
      << "-l             load a json snippet from standard input" << endl
      << endl;
  exit(1);
}

int main(int argc, char** argv) {
  using namespace std;
  int c;

  janosh::Format f = janosh::Bash;

  bool load = false;
  bool set = false;
  bool dump = false;
  bool runTriggers = false;
  bool execTriggers = false;

  string key;
  string value;
  string triggerList;

  while ((c = getopt(argc, argv, "fjbrjsltde:")) != -1) {
    switch (c) {
    case 'f':
      if(string(optarg) == "bash")
        f=janosh::Bash;
      else if(string(optarg) == "json")
        f=janosh::Json;
      else if(string(optarg) == "raw")
        f=janosh::Raw;
      else
        printUsage();
      break;
    case 'j':
      f=janosh::Json;
      break;
    case 'd':
      dump=true;
      break;
    case 'b':
      f=janosh::Bash;
      break;
    case 'r':
      f=janosh::Raw;
      break;
    case 'l':
      load = true;
      break;
    case 's':
      set = true;
      break;
    case 't':
      runTriggers = true;
      break;
    case 'e':
      execTriggers = true;
      triggerList = optarg;
      break;
    case ':':
      printUsage();
      break;
    case '?':
      printUsage();
      break;
    }
  }

  int argnum = argc - optind;
  janosh::Settings settings;
  janosh::Janosh janosh;

  janosh.open(settings.databaseFile.string());
  if(dump) {
    janosh.dump();
    exit(0);
  }

  if(load) {
    janosh.loadJson(std::cin);
  }

  if(set) {
    if(argnum % 2 != 0)
      printUsage();

    janosh::verbose("Set");

    for(int i = optind; i < argc; i+=2)
      janosh.set(argv[i],argv[i + 1]);

    janosh.close();
    if(runTriggers) {
      janosh::verbose("Run triggers");
      using namespace boost;
      if(!fs::exists(settings.triggerFile)) {
        janosh::error("No triggers defined", settings.triggerFile);
      }

      janosh::TriggerBase triggerBase(settings.triggerFile, settings.triggerDirs);
      for(int i = optind; i < argc; i+=2)
        triggerBase.executeTrigger(string(argv[i]));
    }
  } else {
    if(argnum > 0) {
      janosh::verbose("Get");
      bool found_all = true;
      for(int i = optind; i < argc; ++i) {
        key = argv[i];
        found_all = found_all && janosh.get(key, cout, f);
      }

      if(!found_all)
        return 1;
    }
    janosh.close();
  }

  if(execTriggers) {
    janosh::verbose("Exec triggers");
    using namespace boost;
    if(!fs::exists(settings.triggerFile)) {
      janosh::error("No triggers defined", settings.triggerFile);
    }

    janosh::TriggerBase triggers(settings.triggerFile, settings.triggerDirs);
    tokenizer<char_separator<char> > tok(triggerList, char_separator<char>(","));

    BOOST_FOREACH (const string& t, tok) {
      triggers.executeTarget(t);
    }
  }

  return 0;
}
