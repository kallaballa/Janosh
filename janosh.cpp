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
  namespace js = json_spirit;
  namespace fs = boost::filesystem;

  class TriggerBase {
    struct Target {
      string name;
      string command;

      Target(const string& name) :
        name(name) {
      }

      Target(const string& name, const string& command) :
        name(name),
        command(command) {
      }

      int operator()() const {
        return system(command.c_str());
      }

      bool operator<(const Target& other) const {
        return this->name < other.name;
      }
    };

    vector<fs::path> targetDirs;
    map<DBPath, set<Target> > triggers;
    set<Target> targets;
public:
    TriggerBase(const fs::path& config, const vector<fs::path>& targetDirs) :
      targetDirs(targetDirs) {
      load(config);
    }

    void executeTarget(const string& name) {
      auto it = targets.find(name);
      if(it == targets.end())
        error("Target not found", name);

      janosh::verbose("target", name);
      (*it)();
    }

    void executeTrigger(const DBPath p) {
      auto it = triggers.find(p);
      if(it != triggers.end()) {
        BOOST_FOREACH(const Target& t, (*it).second) {
          janosh::verbose("trigger " + t.name, t.command);
          t();
        }
      }
    }

    Target makeTarget(const string& name, const string& command) {
      bool found = false;
      string absCommand;
      string base;
      istringstream(command) >> base;

      BOOST_FOREACH(const fs::path& dir, targetDirs) {
        fs::path entryPath;
        fs::directory_iterator end_iter;

        for(fs::directory_iterator dir_iter(dir);
            !found && (dir_iter != end_iter); ++dir_iter) {

          entryPath = (*dir_iter).path();
          if (entryPath.filename().string() == base) {
            found = true;
            absCommand = dir.string() + command;
          }
        }
      }

      if(!found) {
        error("Target not found",command);
      }

      return Target(name, absCommand);
    }

    void load(const fs::path& config) {
      ifstream is(config.string().c_str());
      load(is);
      is.close();
    }

    void load(std::ifstream& is) {
      try {
        js::Value triggerConf;
        js::read(is, triggerConf);

        BOOST_FOREACH(js::Pair& p, triggerConf.get_obj()) {
          string name = p.name_;
          js::Array arrCmdToTrigger = p.value_.get_array();

          if(arrCmdToTrigger.size() < 2)
            error("Illegal target", name);

          string command = arrCmdToTrigger[0].get_str();
          js::Array arrTriggers = arrCmdToTrigger[1].get_array();

          Target target = makeTarget(name, command);
          targets.insert(target);
          BOOST_FOREACH(const js::Value& v, arrTriggers) {
            triggers[v.get_str()].insert(target);
          }
        }
      } catch (exception& e) {
        error("Unable to load trigger configuration", e.what());
      }
    }
  };

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
