#include "Commands.hpp"
#include <map>
#include <vector>
#include <boost/range.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

using std::map;
using std::vector;
using boost::make_iterator_range;
using boost::tokenizer;
using boost::char_separator;
namespace jh = janosh;

namespace janosh {
  namespace kc = kyotocabinet;
  namespace js = json_spirit;
  namespace fs = boost::filesystem;

  using std::cerr;
  using std::cout;
  using std::endl;
  using std::string;
  using std::vector;
  using std::map;
  using std::istringstream;
  using std::ifstream;
  using std::exception;
  using boost::lexical_cast;

  TriggerBase::TriggerBase(const fs::path& config, const vector<fs::path>& targetDirs) :
    targetDirs(targetDirs) {
    load(config);
  }

  int TriggerBase::executeTarget(const string& name) {
    auto it = targets.find(name);
    if(it == targets.end()) {
      error("Target not found", name);
    }
    LOG_DEBUG_MSG("Execute target", name);
    return system((*it).second.c_str());
  }

  void TriggerBase::executeTrigger(const DBPath p) {
    auto it = triggers.find(p);
    if(it != triggers.end()) {
      BOOST_FOREACH(const string& name, (*it).second) {
        executeTarget(name);
      }
    }
  }

  bool TriggerBase::findAbsoluteCommand(const string& cmd, string& abs) {
    bool found = false;
    string base;
    istringstream(cmd) >> base;

    BOOST_FOREACH(const fs::path& dir, targetDirs) {
      fs::path entryPath;
      fs::directory_iterator end_iter;

      for(fs::directory_iterator dir_iter(dir);
          !found && (dir_iter != end_iter); ++dir_iter) {

        entryPath = (*dir_iter).path();
        if (entryPath.filename().string() == base) {
          found = true;
          abs = dir.string() + cmd;
        }
      }
    }

    return found;
  }

  void TriggerBase::load(const fs::path& config) {
    if(!fs::exists(config)) {
      error("Trigger config doesn't exist", config);
      exit(1);
    }

    ifstream is(config.string().c_str());
    load(is);
    is.close();
  }

  void TriggerBase::load(std::ifstream& is) {
    try {
      js::Value triggerConf;
      js::read(is, triggerConf);

      BOOST_FOREACH(js::Pair& p, triggerConf.get_obj()) {
        string name = p.name_;
        js::Array arrCmdToTrigger = p.value_.get_array();

        if(arrCmdToTrigger.size() < 2){
          error("Illegal target", name);
        }

        string cmd = arrCmdToTrigger[0].get_str();
        js::Array arrTriggers = arrCmdToTrigger[1].get_array();
        string abs;

        if(!findAbsoluteCommand(cmd, abs)) {
          error("Target not found in search path", cmd);
        }

        targets[name] = abs;

        BOOST_FOREACH(const js::Value& v, arrTriggers) {
          triggers[v.get_str()].insert(name);
        }
      }
    } catch (exception& e) {
      error("Unable to load trigger configuration", e.what());
    }
  }


  Settings::Settings() {
    const char* home = getenv ("HOME");
    if (home==NULL) {
      error("Can't find environment variable.", "HOME");
    }
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

  bool Settings::find(const js::Object& obj, const string& name, js::Value& value) {
    auto it = find_if(obj.begin(), obj.end(),
        [&](const js::Pair& p){ return p.name_ == name;});
    if (it != obj.end()) {
      value = (*it).value_;
      return true;
    }
    return false;
  }


  Janosh::Janosh() :
    settings(),
    triggers(settings.triggerFile, settings.triggerDirs) {
  }

  void Janosh::setFormat(Format f) {
    this->format = f;
  }

  Format Janosh::getFormat() {
    return this->format;
  }

  void Janosh::open() {
    // open the database
    if (!db.open(settings.databaseFile.string(), kc::PolyDB::OREADER | kc::PolyDB::OWRITER | kc::PolyDB::OCREATE)) {
      LOG_ERR_MSG("open error", db.error().name());
      exit(2);
    }
  }

  void Janosh::loadJson(const string& jsonfile) {
    std::ifstream is(jsonfile.c_str());
    this->loadJson(is);
    is.close();
  }

  void Janosh::loadJson(std::istream& is) {
    js::Value rootValue;
    js::read(is, rootValue);

    DBPath path;
    load(rootValue, path);
  }

  bool Janosh::print(const DBPath& path, std::ostream& out, Format f) {
    string key = path.str();
    string cvalue;
    LOG_DEBUG_MSG("print", path.str());

    if (path.isContainer()) {
      bool found = db.check(key);

      if(found) {
        kc::DB::Cursor* cur = db.cursor();
        cur->jump(key);

        switch(f) {
          case Json:
            recurse(JsonPrintVisitor(out), cur);
            break;
          case Bash:
            recurse(BashPrintVisitor(out), cur);
            break;
          case Raw:
            recurse(RawPrintVisitor(out), cur);
            break;
        }
        delete cur;
      }

      return found;
    } else {
      string value;
      bool found = db.get(path.str().c_str(), &value);

      if(found) {
        switch(f) {
        case Raw:
        case Json:
          out << value << endl;
          break;
        case Bash:
          out << "\"( [" << key << "]='" << value << "' )\"" << endl;
          break;
        }
      }

      return found;
    }
  }

  void Janosh::set(DBPath dbPath, const string& value, bool check) {
    DBPath dbParent = dbPath.parent();
    string parent = dbParent.str();
    string path = dbPath.str();
    if(check && db.check(path) == -1) {
      if(db.check(parent) == -1) {
        LOG_ERR_MSG("Parent container doesn't exist", path);
        exit(3);
      } else {
        changeContainerSize(dbParent, 1);
      }
    }
    LOG_DEBUG_MSG("Set", path);
    db.set(path, value);
  }

  void Janosh::close() {
    db.close();
  }

  size_t Janosh::remove(const DBPath& path, kc::DB::Cursor* cur) {
    string key = path.str();
    const string& name = path.name();
    string value;
    size_t cnt = 0;

    if(name != "/*") {
      if(!cur) {
        if(!db.get(key, &value))
          return 0;
        cur = db.cursor();
        cur->jump(key);
      }

      if(!cur->remove())
        return 0;

      ++cnt;
    } else {
      key = path.parent().str();

      if(!cur) {
        if(!db.get(key, &value))
          return 0;
        cur = db.cursor();
        cur->jump(key);
        cur->step();
      }
    }

    DBPath rootPath(key);
    EntryType rootType = rootPath.getType(value);

    if (rootType != EntryType::Value) {
      const size_t size = rootPath.getSize(value);
      for (size_t i = 0; i < size; ++i) {
        cur->get(&key, &value, false);
        const DBPath p(key);
        if(p.getType(value) == EntryType::Value) {
          cur->remove();
          ++cnt;
        } else {
          cnt += remove(p, cur);
        }
      }
      setContainerSize(rootPath, 0);

      delete cur;
    } else {
      changeContainerSize(path.parent(), -1);
    }
    return cnt;
  }

  void Janosh::dump() {
    kc::DB::Cursor* cur = db.cursor();
    string key,value;
    cur->jump();
    while(cur->get(&key, &value, true)) {
      std::cout << "key: " << key <<  " value:" << value << endl;
    }
  }

  void Janosh::move(const DBPath& from, const DBPath& to) {
    string fromKey = from.str();
    string toKey = to.str();
    DBPath fromParent = from.parent();
    DBPath toParent = to.parent();

    string v;
    if(!from.isContainer() && !from.isContainer()
        && fromParent.isContainer() && toParent.isContainer() &&
        toParent.str() == fromParent.str() &&
        this->get(fromParent.str(), &v) && fromParent.getType(v) == Array) {

      size_t arraySize = fromParent.getSize(v);
      size_t fromIndex = from.parseIndex();
      size_t toIndex = to.parseIndex();

      if(fromIndex >= arraySize) {
        error("<From> index greater than array size", from.name());
      }

      if(toIndex >= arraySize) {
        error("<To> index greater than array size", from.name());
      }

      string moveValue, value;

      kc::DB::Cursor* cur = db.cursor();
      kc::DB::Cursor* forwardCur = db.cursor();
      if(fromIndex < toIndex) {
        cur->jump(fromKey);
        forwardCur->jump(fromKey);
        forwardCur->step();
      } else {
        cur->jump_back(fromKey);
        forwardCur->jump_back(fromKey);
        forwardCur->step_back();
      }

      cur->get_value(&moveValue,false);

      for(size_t i=0; i < abs(toIndex-fromIndex); ++i) {
        forwardCur->get_value(&value,true);
        cur->set_value_str(value,true);
      }

      cur->set_value_str(moveValue);

      delete cur;
      delete forwardCur;
    } else {
      error("Move is limited on members of the same array", from.str() + "->" + to.str());
    }
  }

  bool Janosh::get(const string& key, string* value, bool check) {
    if(!db.get(key, value)) {
      if(check)
        error("Unknown key", key);
      else
        return false;
    }
    return true;
  }

  void Janosh::setContainerSize(const DBPath& container, const size_t& s, kc::DB::Cursor* cur) {
     string containerValue;
     string strContainer;

     if(!cur) {
       cur = db.cursor();
       cur->jump(container.str());
     }

     cur->get(&strContainer, &containerValue);

     const EntryType& containerType = container.getType(containerValue);
     char t;

     if (containerType == Array)
       t = 'A';
     else if (containerType == Object)
       t = 'O';
     else
       assert(false);

     const string& new_value =
         (boost::format("%c%d") % t % (s)).str();
     cur->set_value_str(new_value);
   }

  void Janosh::changeContainerSize(const DBPath& container, const size_t& by, kc::DB::Cursor* cur) {
    string containerValue;
    string strContainer;
    bool deleteCursor;
    if(!cur) {
      cur = db.cursor();
      cur->jump(container.str());
      deleteCursor = true;
    }

    cur->get(&strContainer, &containerValue);

    const EntryType& containerType = container.getType(containerValue);
    const size_t& containerSize = container.getSize(containerValue);
    char t;

    if (containerType == Array)
      t = 'A';
    else if (containerType == Object)
      t = 'O';
    else
      assert(false);

    const string& new_value =
        (boost::format("%c%d") % t % (containerSize + by)).str();
    cur->set_value_str(new_value);

    if(deleteCursor)
      delete cur;
  }

  void Janosh::load(js::Value& v, DBPath& path) {
    if (v.type() == js::obj_type) {
      load(v.get_obj(), path);
    } else if (v.type() == js::array_type) {
      load(v.get_array(), path);
    } else {
      this->set(path.str(), v.get_str(), false);
    }
  }

  void Janosh::load(js::Object& obj, DBPath& path) {
    path.pushMember(".");
    this->set(path.str(), (boost::format("O%d") % obj.size()).str(), false);
    path.pop();

    BOOST_FOREACH(js::Pair& p, obj) {
      path.pushMember(p.name_);
      load(p.value_, path);
      path.pop();
    }
  }

  void Janosh::load(js::Array& array, DBPath& path) {
    int index = 0;
    path.pushMember(".");
    this->set(path.str(), (boost::format("A%d") % array.size()).str(), false);
    path.pop();

    BOOST_FOREACH(js::Value& v, array){
      path.pushIndex(index++);
      load(v, path);
      path.pop();
    }
  }
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

typedef map<const string, jh::Command*> CommandMap;

CommandMap makeCommandMap(jh::Janosh* janosh) {
  CommandMap cm;
  cm.insert({"load", new jh::LoadCommand(janosh) });
//  cm.insert({"dump", bind(constructor<jh::DumpCommand>(),_1) });
  cm.insert({"set", new jh::SetCommand(janosh) });
  cm.insert({"get", new jh::GetCommand(janosh) });
  cm.insert({"remove", new jh::RemoveCommand(janosh) });
  cm.insert({"move", new jh::MoveCommand(janosh) });
  cm.insert({"dump", new jh::DumpCommand(janosh) });
  cm.insert({"triggers", new jh::TriggerCommand(janosh) });
  cm.insert({"targets", new jh::TargetCommand(janosh) });

  return cm;
}

int main(int argc, char** argv) {
  using namespace std;
  int c;

  janosh::Format f = janosh::Bash;

  bool verbose = false;
  bool execTriggers = false;
  bool execTargets = false;

  string key;
  string value;
  string triggerList;

  while ((c = getopt(argc, argv, "vfjbrte:")) != -1) {
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
    case 'v':
      verbose=true;
      break;
    case 't':
      execTriggers = true;
      break;
    case 'e':
      execTargets = true;
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

  if(argc - optind >= 1) {
    jh::Janosh janosh;
    janosh.setFormat(f);

    if(verbose)
      Logger::init(LogLevel::DEBUG);
    else
      Logger::init(LogLevel::INFO);

    janosh.open();
    string strCmd = string(argv[optind]);
    CommandMap cm = makeCommandMap(&janosh);
    jh::Command* cmd = cm[strCmd];
    if(!cmd)
      janosh.error("Unknown command", strCmd);

    jh::Command::Result r = (*cmd)(make_iterator_range(argv+optind+1, argv+argc));
    LOG_INFO_MSG(r.second, r.first);

    if(strCmd == "set" && execTriggers) {
      vector<const char *> vecTriggers;

      for(size_t i = optind+1; i < argc; i+=2) {
        vecTriggers.push_back(argv[i]);
      }
      (*cm["triggers"])(make_iterator_range(vecTriggers.data(), vecTriggers.data() + vecTriggers.size()));
    }

    if(execTargets) {
      vector<const char *> vecTargets;
      tokenizer<char_separator<char> > tok(triggerList, char_separator<char>(","));

      BOOST_FOREACH (const string& t, tok) {
        vecTargets.push_back(t.c_str());
      }

      (*cm["targets"])(make_iterator_range(vecTargets.data(), vecTargets.data() + vecTargets.size()));
    }

    janosh.close();
  } else {
    printUsage();
  }

  return 0;
}
