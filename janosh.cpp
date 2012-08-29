#include "janosh.hpp"
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>

using namespace std;

using std::cerr;
using std::cout;
using std::endl;
using std::map;
using std::vector;
using std::ifstream;
using std::pair;
using std::string;

namespace janosh {
  namespace js = json_spirit;
  namespace fs = boost::filesystem;

  class Triggers {
    std::map<string, std::function<int ()> > triggerMap;
    Janosh* janosh;
    vector<fs::path> triggerDirs;
  public:
    Triggers(Janosh& janosh, const vector<fs::path>& triggerDirs, const string& filename) :
      janosh(&janosh),
      triggerDirs(triggerDirs) {
      load(filename);
    }

    struct Trigger {
      Janosh* janosh;
      fs::path script;
      vector<DBPath> arguments;

      Trigger(Janosh* janosh, const fs::path& script, const vector<DBPath>& arguments) :
        janosh(janosh),
        script(script),
        arguments(arguments) {
      }

      int operator()() {
        string value;
        std::stringstream ss;

        ss << script.string() << " \"( ";

        BOOST_FOREACH(const DBPath& dbpath, arguments) {
          janosh->get(dbpath, value);
          replace(value.begin(), value.end(), '\n', ' ');
          ss << '[' << dbpath.str() << "]='" << value << "' ";
        }
        ss << ")\"";

        return system(ss.str().c_str());
      }
    };

    void execute(const string& name) {
      auto it = triggerMap.find(name);
      if(it == triggerMap.end())
        error("trigger not found", name);

      (*it).second();
    }

    void add(const string& name, const string& script, const vector<DBPath>& arguments) {
      bool found = false;
      fs::path scriptPath;
      string base;
      istringstream(script) >> base;

      BOOST_FOREACH(const fs::path& dir, triggerDirs) {
        fs::path entryPath;
        fs::directory_iterator end_iter;

        for(fs::directory_iterator dir_iter(dir);
            !found && (dir_iter != end_iter); ++dir_iter) {

          entryPath = (*dir_iter).path();
          if (entryPath.filename().string() == base) {
            found = true;
            scriptPath = fs::path(dir.string() + script);
          }
        }
      }

      if(!found) {
        error("script not found",script);
      }

      triggerMap[name] = Trigger(janosh,scriptPath,arguments);
    }

    void load(const string& filename) {
      ifstream is(filename.c_str());
      load(is);
      is.close();
    }

    void load(std::ifstream& is) {
      try {
        js::Value triggerConf;
        js::read(is, triggerConf);

        BOOST_FOREACH(js::Pair& p, triggerConf.get_obj()) {
          string name = p.name_;
          js::Array arr = p.value_.get_array();
          string script = arr[0].get_str();
          js::Array args = arr[1].get_array();
          vector<DBPath> vecArgs;

          BOOST_FOREACH(const js::Value& v, args) {
            vecArgs.push_back(v.get_str());
          }

          this->add(name, script, vecArgs);
        }
      } catch (exception& e) {
        error("Unable to load trigger configuration", e.what());
      }
    }
  };

  class Settings {
  public:
    string janoshFile;
    string triggerFile;
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

      this->janoshFile = dir.string() + "janosh.json";
      this->triggerFile = dir.string() + "triggers.json";

      if(!fs::exists(janoshFile)) {
        error("janosh configuration not found: ", janoshFile);
      } else {
        js::Value janoshConf;

        try{
          ifstream is(janoshFile);
          js::read(is,janoshConf);
          js::Object jObj = janoshConf.get_obj();
          js::Value v;

          if(find(jObj, "triggerDirectories", v)) {
            BOOST_FOREACH (const js::Value& vDir, v.get_array()) {
              triggerDirs.push_back(fs::path(vDir.get_str()));
            }
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
  bool load = false;
  bool runTriggers = false;

  string key;
  string value;
  string triggerList;

  while ((c = getopt(argc, argv, "lt:")) != -1) {
    switch (c) {
    case 'l':
      load = true;
      break;
    case 't':
      runTriggers = true;
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

  namespace fs = boost::filesystem;
  janosh::Settings settings;
  janosh::Janosh janosh;
  janosh.open("db.chk");

  if(load) {
    janosh.loadJson(std::cin);
  }
  else {
    if(argc - optind > 1) {
      printUsage();
    } else if(argc - optind == 1) {
      key = argv[optind];
      string value;
      janosh.get(key,value);
      cout << value << endl;
    }
  }

  if(runTriggers) {
    using namespace boost;
    if(!fs::exists(settings.triggerFile)) {
      janosh::error("No triggers defined", settings.triggerFile);
    }

    janosh::Triggers triggers(janosh, settings.triggerDirs, settings.triggerFile);
    tokenizer<char_separator<char> > tok(triggerList, char_separator<char>("."));

    BOOST_FOREACH (const string& t, tok) {
      triggers.execute(t);
    }
  }
}
