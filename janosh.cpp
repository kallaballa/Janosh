#include "Commands.hpp"
#include <map>
#include <vector>
#include <boost/range.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/functional/hash.hpp>

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
    this->logFile = fs::path(dir.string() + "janosh.log");

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
    if (!DBPath::db.open(settings.databaseFile.string(), kc::PolyDB::OREADER | kc::PolyDB::OWRITER | kc::PolyDB::OCREATE)) {
      LOG_ERR_MSG("open error", DBPath::db.error().name());
      exit(2);
    }
  }

  void Janosh::close() {
    DBPath::db.close();
  }

  size_t Janosh::loadJson(const string& jsonfile) {
    std::ifstream is(jsonfile.c_str());
    size_t cnt = this->loadJson(is);
    is.close();
    return cnt;
  }

  size_t Janosh::loadJson(std::istream& is) {
    js::Value rootValue;
    js::read(is, rootValue);

    DBPath path;
    return load(rootValue, path);
  }

  size_t Janosh::print(DBPath path, std::ostream& out) {
    return print(path.read(), out);
  }

  size_t Janosh::print(Cursor cur, std::ostream& out) {
    DBPath path(cur);
    size_t cnt = 1;

    LOG_DEBUG_MSG("print", path.key());

    if(!path.exists())
      error("Path not found", path.key());

    if (path.isContainer()) {
      switch(this->getFormat()) {
        case Json:
          cnt = recurse(cur, JsonPrintVisitor(out));
          break;
        case Bash:
          cnt = recurse(cur, BashPrintVisitor(out));
          break;
        case Raw:
          cnt = recurse(cur, RawPrintVisitor(out));
          break;
      }
    } else {
      string value;

      switch(this->getFormat()) {
        case Raw:
        case Json:
          out << path.val() << endl;
          break;
        case Bash:
          out << "\"( [" << path.key() << "]='" << path.val() << "' )\"" << endl;
          break;
      }
    }
    return cnt;
  }

  size_t Janosh::makeArray(DBPath target, size_t size) {
    target.prune();
    assert(target.isContainer());
    assert(!target.exists());
    return DBPath::db.add(target.key(), "A" + lexical_cast<string>(size));
  }

  size_t Janosh::makeObject(DBPath target, size_t size) {
    target.prune();
    assert(target.isContainer());
    assert(!target.exists());
    return DBPath::db.add(target.key(), "O" + lexical_cast<string>(size));
  }

  size_t Janosh::makeDirectory(DBPath target, EntryType type, size_t size) {
    target.prune();
    assert(!target.exists());

    if(type == Array) {
      return makeArray(target, size);
    } else if (type == Object) {
      return makeObject(target, size);
    } else
      assert(false);
    return 0;
  }

  size_t Janosh::add(DBPath path, const string& value) {
    LOG_DEBUG_MSG("add", path.key());
    DBPath parent = path.parent();
    path.prune();
    parent.prune();
    assert(path.isValue() && !path.exists());
    assert(!parent.isArray() || path.parseIndex() <= parent.getSize());

    if(DBPath::db.add(path.key(), value)) {
      if(!path.isRoot())
        changeContainerSize(path.parent(), 1);

      return true;
    } else {
      return false;
    }
  }

  size_t Janosh::replace(DBPath path, const string& value) {
    LOG_DEBUG_MSG("replace", path.key());
    DBPath parent = path.parent();
    path.prune();
    parent.prune();
    assert(path.isValue() && path.exists());
    assert(parent.isArray() || parent.isObject());

    return DBPath::db.replace(path.key(), value);
  }

  size_t Janosh::set(DBPath path, const string& value) {
    LOG_DEBUG_MSG("Set", path.key());
    DBPath parent = path.parent();
    path.prune();
    parent.prune();
    assert(path.isValue());
    assert(parent.isObject() || (parent.isArray() && path.parseIndex() < parent.getSize()));
    bool isnew = !path.exists();
    if(DBPath::db.set(path.key(), value)) {
      if(isnew)
        changeContainerSize(parent, 1);
      return true;
    }
    return false;
  }

  size_t Janosh::set(Cursor cur, const string& value) {
    LOG_DEBUG_MSG("Set", DBPath(cur).key());
    return cur.setValue(value);
  }

  size_t Janosh::remove(DBPath p) {
    LOG_DEBUG_MSG("remove", p.key());
    p.prune();

    if(p.isWildcard()) {
      return this->removeChildren(p.getCursor());
    } else
      return this->remove(p.getCursor(), 1);
  }

  size_t Janosh::remove(Cursor cur, size_t n) {
    if(!cur.isValid())
      return 0;

    size_t cnt = 0;

    for(size_t i = 0; i < n; ++i) {
      DBPath path(cur);
      EntryType rootType = path.getType();

      if (rootType != EntryType::Value) {
        cur.remove();
        ++cnt;
        const size_t size = path.getSize();
        for (size_t j = 0; j < size; ++j) {
          DBPath p(cur);
          if(p.getType() == EntryType::Value) {
            cur.remove();
            ++cnt;
          } else {
            cnt += remove(cur);
          }
        }
      } else {
        cur.remove();
        ++cnt;
      }

      changeContainerSize(path.parent(), -1);
    }
    return cnt;
  }

  size_t Janosh::removeChildren(Cursor cur) {
    LOG_DEBUG_MSG("Remove Children", DBPath(cur).key());
    if(!cur.isValid())
      return 0;

    DBPath rootPath(cur);
    cur.step();
    return this->remove(cur, rootPath.getSize());
  }

  size_t Janosh::dump() {
    kc::DB::Cursor* cur = DBPath::db.cursor();
    string key,value;
    cur->jump();
    size_t cnt = 0;
    while(cur->get(&key, &value, true)) {
      std::cout << "key: " << key <<  " value:" << value << endl;
      ++cnt;
    }
    delete cur;
    return cnt;
  }

  size_t Janosh::hash() {
    kc::DB::Cursor* cur = DBPath::db.cursor();
    string key,value;
    cur->jump();
    size_t cnt = 0;
    boost::hash<string> hasher;
    size_t h = 0;
    while(cur->get(&key, &value, true)) {
      h = hasher(lexical_cast<string>(h) + key + value);
      ++cnt;
    }
    std::cout << h << std::endl;
    delete cur;
    return cnt;
  }

  size_t Janosh::truncate() {
    if(DBPath::db.clear())
      return makeObject("/.");
    else
      return false;
  }

  size_t Janosh::size(DBPath path) {
    path.prune();
    if(!path.isContainer())
      error("size is limited to containers", path.key());

    return path.getSize();
  }

  size_t Janosh::append(const string& value, Cursor destCursor) {
    DBPath dest(destCursor);
    dest.prune();
    if(!dest.isContainer() || dest.getType() != Array)
      error("append is limited to arrays", dest.key());

    DBPath target(dest.basePath() + '/' + lexical_cast<string>(dest.getSize()));
    assert(DBPath::db.add(target.key(), value));
    destCursor = target.getCursor(destCursor);
    return 1;
  }

  size_t Janosh::append(vector<string>::const_iterator begin, vector<string>::const_iterator end, Cursor destCursor) {
    DBPath dest(destCursor);
    dest.prune();
    if(!dest.isContainer() || dest.getType() != Array)
      error("append is limited to dest arrays", dest.key());

    size_t s = dest.getSize();
    string basePath = dest.basePath();

    size_t cnt = 0;
    for(; begin != end; ++begin) {
      DBPath target = dest.makeChild(s + cnt);
      assert(DBPath::db.add(target.key(), *begin));
      ++cnt;
    }

    setContainerSize(dest, s + cnt);
    return cnt;
  }

  size_t Janosh::append(Cursor srcCur, Cursor destCur, size_t n) {
    DBPath dest(destCur);

    if(dest.isContainer() && dest.getType() != Array)
      error("append is limited to arrays", dest.key());

    size_t s = dest.getSize();
    string basePath = dest.basePath();
    size_t cnt = 0;
    string value;

    for(; cnt < n; ++cnt) {
      srcCur.getValue(value);
      DBPath target = dest.makeChild(s + cnt);
      assert(DBPath::db.add(target.key(), value));
    }

    setContainerSize(dest, s + cnt);
    return cnt;
  }

  size_t Janosh::appendChildren(Cursor srcCur, Cursor destCur) {
    DBPath src(srcCur);
    Cursor targetCursor = src.getCursor();
    targetCursor.step();
    return append(targetCursor, destCur, src.getSize());
  }

  size_t Janosh::copy(Cursor srcCur, Cursor destCur) {
    DBPath src(srcCur);
    DBPath dest(destCur);
    LOG_DEBUG_MSG("copy", src.key() + "->" + dest.key());

    if(dest.isWildcard())
      error("Destination can't be a wildcard", dest.key());

    if(src == dest)
      return 0;

    if(src.isWildcard()) {
      DBPath target = dest.makeDirectory();
      remove(dest);
      this->makeDirectory(target, src.getType());
      destCur = target.getCursor();

      if (!dest.isContainer()) {
        remove(destCur);

        if(src.getType() == Array) {
          assert(makeArray(target, 0));
        } else if(src.getType() == Object) {
          assert(makeObject(target, 0));
        }
      }

      destCur = target.getCursor();
      return this->appendChildren(srcCur, destCur);
    } else if(src.isContainer()) {
      DBPath target = dest.makeDirectory();
      this->makeDirectory(target, src.getType());

      if(dest.isContainer()) {
        remove(dest.makeWildcard());
        setContainerSize(destCur, 0);
      } else {
        remove(destCur);
      }

      destCur = target.getCursor();
      return this->appendChildren(srcCur, destCur);
    } else {
      if(dest.isContainer()) {
        //add first to make sure the cursor will step to the newly created entry on remove
        bool success = this->add(dest.basePath(), src.val());
        remove(destCur);
        destCur.step_back();
        return success;
      } else {
        return this->set(destCur, src.val());
      }
    }
  }

  size_t Janosh::shift(Cursor srcCur, Cursor destCur) {
    DBPath src(srcCur);
    DBPath dest(destCur);
    LOG_DEBUG_MSG("shift", src.key() + "->" + dest.key());

    DBPath srcParent = src.parent();
    DBPath destParent = dest.parent();
    srcParent.prune();
    destParent.prune();

    if(srcParent.getType() != EntryType::Array) {
      error("Move is limited on arrays", srcParent.key());
    }

    if(destParent.getType() != EntryType::Array) {
      error("Move is limited on arrays", destParent.key());
    }

    size_t destSize = destParent.getSize();
    size_t srcIndex = src.parseIndex();
    size_t destIndex = dest.parseIndex();

    if(srcIndex >= destSize) {
      error("<From> index greater than array size", src.name());
    }

    if(destIndex >= destSize) {
      error("<To> index greater than array size", src.name());
    }

    if(srcParent == destParent) {
      bool back = srcIndex > destIndex;
      Cursor forwardCur = src.getCursor();
      Cursor backCur = src.getCursor();

      if(back) {
        assert(forwardCur.previous());
      } else {
        assert(forwardCur.next());
      }

      Cursor tmpCursor;

      if(src.isContainer()) {
        DBPath tmp = destParent.makeChild(destSize).makeDirectory();
        makeDirectory(tmp, src.getType());
        Cursor tmpCursor = tmp.getCursor();
        this->append(src.getCursor(), tmpCursor, src.getSize());
        remove(src);
      } else {
        DBPath tmp = destParent.makeChild(destSize);
        this->add(tmp, src.val());
        tmpCursor = tmp.getCursor();
      }

      for(size_t i=0; i < abs(destIndex - srcIndex); ++i) {
        copy(forwardCur, backCur);

        if(back) {
          assert(backCur.previous());
          assert(forwardCur.previous());
        } else {
          assert(backCur.next());
          assert(forwardCur.next());
        }
      }

      copy(tmpCursor,destCur);
      remove(tmpCursor);
    } else
      assert(false);

    destCur = srcCur;
    srcCur = dest.getCursor();

    return 1;
  }

  void Janosh::setContainerSize(Cursor cur, const size_t s) {
    DBPath container(cur);
    LOG_DEBUG_MSG("set container size" + container.key(), s);
    string containerValue;
    string strContainer;

    const EntryType& containerType = container.getType();
    char t;

    if (containerType == Array)
     t = 'A';
    else if (containerType == Object)
     t = 'O';
    else
     assert(false);

    const string& new_value =
       (boost::format("%c%d") % t % (s)).str();
    cur.setValue(new_value);
  }

  void Janosh::changeContainerSize(Cursor cur, const size_t by) {
    DBPath container(cur);
    setContainerSize(cur, container.getSize() + by);
  }

  size_t Janosh::load(DBPath path, const string& value) {
    LOG_DEBUG_MSG("add", path.key());
    return DBPath::db.add(path.key(), value);
  }

  size_t Janosh::load(js::Value& v, DBPath& path) {
    size_t cnt = 0;
    if (v.type() == js::obj_type) {
      cnt+=load(v.get_obj(), path);
    } else if (v.type() == js::array_type) {
      cnt+=load(v.get_array(), path);
    } else {
      cnt+=this->load(path, v.get_str());
    }
    return cnt;
  }

  size_t Janosh::load(js::Object& obj, DBPath& path) {
    size_t cnt = 0;
    path.pushMember(".");
    cnt+=this->load(path, (boost::format("O%d") % obj.size()).str());
    path.pop();

    BOOST_FOREACH(js::Pair& p, obj) {
      path.pushMember(p.name_);
      cnt+=load(p.value_, path);
      path.pop();
      ++cnt;
    }

    return cnt;
  }

  size_t Janosh::load(js::Array& array, DBPath& path) {
    size_t cnt = 0;
    int index = 0;
    path.pushMember(".");
    cnt+=this->load(path, (boost::format("A%d") % array.size()).str());
    path.pop();

    BOOST_FOREACH(js::Value& v, array){
      path.pushIndex(index++);
      cnt+=load(v, path);
      path.pop();
      ++cnt;
    }
    return cnt;
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
  cm.insert({"add", new jh::AddCommand(janosh) });
  cm.insert({"replace", new jh::ReplaceCommand(janosh) });
  cm.insert({"set", new jh::SetCommand(janosh) });
  cm.insert({"get", new jh::GetCommand(janosh) });
  cm.insert({"copy", new jh::CopyCommand(janosh) });
  cm.insert({"remove", new jh::RemoveCommand(janosh) });
  cm.insert({"shift", new jh::ShiftCommand(janosh) });
  cm.insert({"append", new jh::AppendCommand(janosh) });
  cm.insert({"dump", new jh::DumpCommand(janosh) });
  cm.insert({"size", new jh::SizeCommand(janosh) });
  cm.insert({"triggers", new jh::TriggerCommand(janosh) });
  cm.insert({"targets", new jh::TargetCommand(janosh) });
  cm.insert({"truncate", new jh::TruncateCommand(janosh) });
  cm.insert({"mkarr", new jh::MakeArrayCommand(janosh) });
  cm.insert({"mkobj", new jh::MakeObjectCommand(janosh) });
  cm.insert({"hash", new jh::HashCommand(janosh) });

  return cm;
}

kyotocabinet::TreeDB janosh::DBPath::db;

#include <boost/generator_iterator.hpp>


int main(int argc, char** argv) {
  using namespace std;
  int c;

  janosh::Format f = janosh::Bash;

  bool verbose = false;
  bool execTriggers = false;
  bool execTargets = false;

  string key;
  string value;
  string targetList;

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
      targetList = optarg;
      break;
    case ':':
      printUsage();
      break;
    case '?':
      printUsage();
      break;
    }
  }
  jh::Janosh janosh;
  janosh.setFormat(f);

  if(verbose)
    Logger::init(LogLevel::DEBUG);
  else
    Logger::init(LogLevel::INFO);

  janosh.open();
  CommandMap cm = makeCommandMap(&janosh);

  if(argc - optind >= 1) {
    string strCmd = string(argv[optind]);
    jh::Command* cmd = cm[strCmd];
    if(!cmd)
      janosh.error("Unknown command", strCmd);

    vector<string> vecParams;
    for(size_t i = optind+1; i < argc; ++i) {
      vecParams.push_back(argv[i]);
    }

    jh::Command::Result r = (*cmd)(vecParams);
    LOG_INFO_MSG(r.second, r.first);

    janosh.close();
    if(strCmd == "set" && execTriggers) {
      vector<string> vecTriggers;

      for(size_t i = optind+1; i < argc; i+=2) {
        vecTriggers.push_back(argv[i]);
      }
      (*cm["triggers"])(vecTriggers);
    }

    return r.first ? 0 : 1;
  } else if(!execTargets){
    printUsage();
  }

  janosh.close();

  if(execTargets) {
     vector<string> vecTargets;
     tokenizer<char_separator<char> > tok(targetList, char_separator<char>(","));

     BOOST_FOREACH (const string& t, tok) {
       vecTargets.push_back(t);
     }

     (*cm["targets"])(vecTargets);
   }

  return 0;
}
