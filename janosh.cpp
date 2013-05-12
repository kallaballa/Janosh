#include "janosh.hpp"
#include "commands.hpp"

using std::string;
using std::map;
using std::vector;
using boost::make_iterator_range;
using boost::tokenizer;
using boost::char_separator;

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
      warn("Target not found", name);
      return -1;
    }
    LOG_INFO_MSG("Execute target", name);
    return system((*it).second.c_str());
  }

  void TriggerBase::executeTrigger(const Path& p) {
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
            triggerDirs.push_back(fs::path(vDir.get_str() + "/"));
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
		settings_(),
        triggers_(settings_.triggerFile, settings_.triggerDirs),
        cm(makeCommandMap(this)) {
  }

  Janosh::~Janosh() {
  }

  void Janosh::setFormat(Format f) {
    this->format = f;
  }

  Format Janosh::getFormat() {
    return this->format;
  }

  void Janosh::open(bool readOnly=false) {
    // open the database

    uint32_t mode;
    if(readOnly)
      mode = kc::PolyDB::OAUTOTRAN | kc::PolyDB::OREADER;
    else
      mode = kc::PolyDB::OTRYLOCK | kc::PolyDB::OAUTOTRAN | kc::PolyDB::OREADER | kc::PolyDB::OWRITER | kc::PolyDB::OCREATE;
    while (!Record::db.open(settings_.databaseFile.string(),  mode)) {
      boost::this_thread::sleep(boost::posix_time::millisec(20));
      LOG_ERR_MSG("open error", Record::db.error().name());
    }
    open_ = true;
  }

  bool Janosh::isOpen() {
    return this->open_;
  }

  void Janosh::close() {
    if(isOpen()) {
      open_ = false;
      Record::db.close();
    }
  }

  void Janosh::printException(std::exception& ex) {
    std::cerr << "Exception: " << ex.what() << std::endl;
  }

  void Janosh::printException(janosh::janosh_exception& ex) {
    using namespace boost;

    std::cerr << "Exception: " << std::endl;
  #if 0
    std::cerr << boost::trace(ex) << std::endl;
  #endif
    if(auto const* m = get_error_info<msg_info>(ex) )
      std::cerr << "message: " << *m << std::endl;

    if(auto const* vs = get_error_info<string_info>(ex) ) {
      for(auto it=vs->begin(); it != vs->end(); ++it) {
        std::cerr << "info: " << *it << std::endl;
      }
    }

    if(auto const* ri = get_error_info<record_info>(ex))
      std::cerr << ri->first << ": " << ri->second << endl;

    if(auto const* pi = get_error_info<path_info>(ex))
      std::cerr << pi->first << ": " << pi->second << std::endl;

    if(auto const* vi = get_error_info<value_info>(ex))
      std::cerr << vi->first << ": " << vi->second << std::endl;
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

    Path path;
    return load(rootValue, path);
  }


  /**
   * Recursively traverses a record and prints it out.
   * @param rec The record to print out.
   * @param out The output stream to write to.
   * @return number of total records affected.
   */
  size_t Janosh::get(Record rec, std::ostream& out) {
    rec.fetch();
    size_t cnt = 1;

    LOG_DEBUG_MSG("print", rec.path());

    if(!rec.exists()) {
      throw janosh_exception() << record_info({"Path not found", rec});
    }

    if (rec.isDirectory()) {
      switch(this->getFormat()) {
        case Json:
          cnt = recurse(rec, JsonPrintVisitor(out));
          break;
        case Bash:
          cnt = recurse(rec, BashPrintVisitor(out));
          break;
        case Raw:
          cnt = recurse(rec, RawPrintVisitor(out));
          break;
      }
    } else {
      string value;

      switch(this->getFormat()) {
        case Raw:
        case Json:
          out << rec.value() << endl;
          break;
        case Bash:
          out << "\"( [" << rec.path() << "]='" << rec.value() << "' )\"" << endl;
          break;
      }
    }
    return cnt;
  }

  /**
   * Creates a temporary record with the given type.
   * @param t the type of the record to create.
   * @return the created record
   */
  Record Janosh::makeTemp(const Value::Type& t) {
    Record tmp("/tmp/.");

    if(!tmp.fetch().exists()) {
      makeDirectory(tmp, Value::Array, 0);
      tmp.fetch();
    }

    if(t == Value::String) {
      tmp = tmp.path().withChild(tmp.getSize());
      this->add(tmp, "");
    } else {
      tmp = tmp.path().withChild(tmp.getSize()).asDirectory();
      this->makeDirectory(tmp, t, 0);
    }
    return tmp;
  }

  /**
   * Creates an array with given size. Optional performs a bounds check.
   * @param target the array record to create.
   * @param size the size of the target array.
   * @param bounds perform bounds check if true.
   * @return 1 if successful, 0 if not
   */
  size_t Janosh::makeArray(Record target, size_t size, bool bounds) {
    JANOSH_TRACE({target}, size);
    target.fetch();
    Record base = Record(target.path().basePath());
    base.fetch();
    if(!target.isDirectory() || target.exists() || base.exists()) {
      throw janosh_exception() << record_info({"Invalid target",target});
    }

    if(bounds && !boundsCheck(target)) {
      throw janosh_exception() << record_info({"Out of array bounds",target});
    }
    changeContainerSize(target.parent(), 1);
    return Record::db.add(target.path(), "A" + lexical_cast<string>(size)) ? 1 : 0;
  }

  /**
   * Creates an object with given size.
   * @param target the object record to create.
   * @param size the size of the target object.
   * @return 1 if successful, 0 if not
   */
  size_t Janosh::makeObject(Record target, size_t size) {
    JANOSH_TRACE({target}, size);
    target.fetch();
    Record base = Record(target.path().basePath());
    base.fetch();
    if(!target.isDirectory() || target.exists() || base.exists()) {
      throw janosh_exception() << record_info({"Invalid target",target});
    }

    if(!boundsCheck(target)) {
      throw janosh_exception() << record_info({"Out of array bounds", target});
    }

    if(!target.path().isRoot())
      changeContainerSize(target.parent(), 1);
    return Record::db.add(target.path(), "O" + lexical_cast<string>(size)) ? 1 : 0;
  }


  /**
   * Creates an either and array or an object with given size.
   * @param target the directory record to create.
   * @param type either Value::Array or Value::Object.
   * @param size the size of the target directory.
   * @return 1 if successful, 0 if not
   */
  size_t Janosh::makeDirectory(Record target, Value::Type type, size_t size) {
    JANOSH_TRACE({target}, size);
    if(type == Value::Array) {
      return makeArray(target, size);
    } else if (type == Value::Object) {
      return makeObject(target, size);
    } else
      assert(false);

    return 0;
  }

  /**
   * Adds a record with the given value to the database.
   * Does not modify an existing record.
   * @param dest destination record
   * @return 1 if successful, 0 if not
   */
  size_t Janosh::add(Record dest, const string& value) {
    JANOSH_TRACE({dest}, value);

    if(!dest.isValue() || dest.exists()) {
      throw janosh_exception() << record_info({"Invalid target", dest});
    }

    if(!boundsCheck(dest)) {
      throw janosh_exception() << record_info({"Out of array bounds",dest});
    }

    if(Record::db.add(dest.path(), value)) {
//      if(!dest.path().isRoot())
        changeContainerSize(dest.parent(), 1);
      return 1;
    } else {
      return 0;
    }
  }

  /**
   * Relaces a the value of a record.
   * If the destination record exists create it.
   * @param dest destination record
   * @return 1 if successful, 0 if not
   */
  size_t Janosh::replace(Record dest, const string& value) {
    JANOSH_TRACE({dest}, value);
    dest.fetch();

    if(!dest.isValue() || !dest.exists()) {
      throw janosh_exception() << record_info({"Invalid target", dest});
    }

    if(!boundsCheck(dest)) {
      throw janosh_exception() << record_info({"Out of array bounds", dest});
    }

    return Record::db.replace(dest.path(), value);
  }


  /**
   * Relaces a destination record with a source record to the new path.
   * If the destination record exists replaces it.
   * @param src source record. Points to the destination record after moving.
   * @param dest destination record.
   * @return 1 if successful, 0 if not
   */
  size_t Janosh::replace(Record& src, Record& dest) {
    JANOSH_TRACE({src, dest});
    src.fetch();
    dest.fetch();

    if(src.isRange() || !src.exists() || !dest.exists()) {
      throw janosh_exception() << record_info({"Invalid target", dest});
    }

    if(!boundsCheck(dest)) {
      throw janosh_exception() << record_info({"Out of array bounds", dest});
    }

    Record target;
    size_t r;
    if(dest.isDirectory()) {
      if(src.isDirectory()) {
        target = dest.path();
        Record wildcard = src.path().asWildcard();
        r = this->copy(wildcard,target);
      } else {
        target = dest.path().basePath();
        remove(dest, false);
        r = this->copy(src,target);
      }

      dest = target;
    } else {
      if(src.isDirectory()) {
        Record target = dest.path().asDirectory();
        Record wildcard = src.path().asWildcard();
        remove(dest, false);
        makeDirectory(target, src.getType());
        r = this->append(wildcard, target);
        dest = target;

      } else {
        r = Record::db.replace(dest.path(), src.value());
      }
    }

    src.fetch();
    dest.fetch();

    return r;
  }

  /**
   * Copies source record value to the destination record - eventually removing the source record.
   * If the destination record exists replaces it.
   * @param src source record. Points to the destination record after moving.
   * @param dest destination record.
   * @return number of copied records
   */
  size_t Janosh::move(Record& src, Record& dest) {
    JANOSH_TRACE({src, dest});
    src.fetch();
    dest.fetch();

    if(src.isRange() || !src.exists()) {
      throw janosh_exception() << record_info({"Invalid src", src});
    }

    if(dest.path().isRoot()) {
      throw janosh_exception() << record_info({"Invalid target", dest});
    }

    if(!boundsCheck(dest)) {
      throw janosh_exception() << record_info({"Out of array bounds", dest});
    }

    Record target;
    size_t r;
    if(dest.isDirectory()) {
      if(src.isDirectory())
        target = dest;
      else
        target = dest.path().basePath();

      if(dest.exists())
        remove(dest);

      r = this->copy(src,target);
      dest = target;
    } else {
      if(src.isDirectory()) {
        target = dest.parent();

        if(dest.exists())
          remove(dest);

        r = this->copy(src, target);
        dest = target;
      } else {
        r = Record::db.replace(dest.path(), src.value());
      }
    }
    remove(src);
    src = dest;

    return r;
  }


  /**
   * Sets/replaces the value of a record. If no record exists, creates the record with corresponding value.
   * @param rec The record to manipulate
   * @return 1 if successful, 0 if not
   */
  size_t Janosh::set(Record rec, const string& value) {
    JANOSH_TRACE({rec}, value);

    if(!rec.isValue()) {
      throw janosh_exception() << record_info({"Invalid target", rec});
    }

    if(!boundsCheck(rec)) {
      throw janosh_exception() << record_info({"Out of array bounds", rec});
    }

    rec.fetch();
    if (rec.exists())
      return replace(rec, value);
    else
      return add(rec, value);
  }


  /**
   * Recursivley removes a record from the database.
   * @param rec The record to remove. Points to the next record in the database after removal.
   * @return number of total records affected.
   */
  size_t Janosh::remove(Record& rec, bool pack) {
    JANOSH_TRACE({rec});
    if(!rec.isInitialized())
      return 0;
 
    size_t n = rec.fetch().getSize();
    size_t cnt = 0;

    Record parent = rec.parent();
    Path targetPath = rec.path();
    Record target(targetPath);
    parent.fetch();
    target.fetch();

    if(target.isDirectory())
      rec.step();

    for(size_t i = 0; i < n; ++i) {
       if(!rec.isDirectory()) {
         rec.remove();
         ++cnt;
       } else {
         remove(rec,false);
       }
     }

    if(target.isDirectory()) {
      target.remove();
      changeContainerSize(parent, -1);
    } else {
      changeContainerSize(parent, cnt * -1);
    }

    if(pack && parent.isArray()) {
      parent.read();
      Record child = Record(parent.path()).fetch();
      size_t left = parent.getSize();
      child.step();
      for(size_t i = 0; i < left; ++i) {
        child.fetch();

        if(child.parent().path() != parent.path()) {
          throw db_exception() << record_info({"corrupted array detected", parent});
        }
        if(child.getIndex() > i) {
          Record indexPos(parent.path().withChild(i));
          copy(child, indexPos);
          child.remove();
        } else {
          child.next();
        }
      }
      setContainerSize(parent, left);
    }

    rec = target;

    return cnt;
  }

  /**
   * Prints all records to cout in raw format
   * @return number of total records printed.
   */
  size_t Janosh::dump() {
    kc::DB::Cursor* cur = Record::db.cursor();
    string key,value;
    cur->jump();
    size_t cnt = 0;

    while(cur->get(&key, &value, true)) {
      std::cout << "path:" << Path(key).pretty() <<  " value:" << value << endl;
      ++cnt;
    }
    delete cur;
    return cnt;
  }

  /**
   * Calculates a hash value over all records
   * @return number of total records hashed.
   */
  size_t Janosh::hash() {
    kc::DB::Cursor* cur = Record::db.cursor();
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

  /**
   * Clears an initializes the database.
   * @return 1 on success, 0 on fail
   */
  size_t Janosh::truncate() {
    if(Record::db.clear())
      return Record::db.add("/!", "O" + lexical_cast<string>(0)) ? 1 : 0;
    else
      return false;
  }

  /**
   * Returns the size of a directory record
   * @param rec the directory record
   * @return 1 on success, 0 on fail
   */
  size_t Janosh::size(Record rec) {
    if(!rec.isDirectory()) {
      throw janosh_exception() << record_info({"size is limited to containers", rec});
    }

    return rec.fetch().getSize();
  }

  /**
   * Append string values from an iterator range to an array record
   * @param begin the begin iterator
   * @param end the end iterator
   * @param rec the array record
   * @return number of values appended
   */
  size_t Janosh::append(vector<string>::const_iterator begin, vector<string>::const_iterator end, Record dest) {
    JANOSH_TRACE({dest});
    dest.fetch();

    if(!dest.isArray()) {
      throw janosh_exception() << record_info({"append is limited to arrays", dest});
    }

    size_t s = dest.getSize();
    size_t cnt = 0;

    for(; begin != end; ++begin) {
      if(!Record::db.add(dest.path().withChild(s + cnt), *begin)) {
        throw janosh_exception() << record_info({"Failed to add target", dest});
      }
      ++cnt;
    }

    setContainerSize(dest, s + cnt);
    return cnt;
  }

  size_t Janosh::append(Record& src, Record& dest) {
    JANOSH_TRACE({src,dest});

    if(!dest.isDirectory()) {
      throw janosh_exception() << record_info({"append is limited to directories", dest});
    }

    src.fetch();
    dest.fetch();

    size_t n = src.getSize();
    size_t s = dest.getSize();
    size_t cnt = 0;
    string path;
    string value;

    for(; cnt < n; ++cnt) {
      if(cnt > 0)
        src.next();

      src.read();
      if(src.isAncestorOf(dest)) {
        throw janosh_exception() << record_info({"can't append an ancestor", src});
      }

      if(src.isDirectory()) {
        Record target;
        if(dest.isObject()) {
          target = dest.path().withChild(src.path().name()).asDirectory();
        } else if(dest.isArray()) {
          target = dest.path().withChild(s + cnt).asDirectory();
        } else {
          throw janosh_exception() << record_info({"can't append to a value", dest});
        }

        if(src.isAncestorOf(target)) {
          throw janosh_exception() << record_info({"can't append an ancestor", src});
        }

        if(!makeDirectory(target, src.getType())) {
          throw janosh_exception() << record_info({"failed to create directory", target});
        }

        Record wildcard = src.path().asWildcard();
        if(!append(wildcard, target)) {
          throw janosh_exception() << record_info({"failed to append values", target});
        }
      } else {
        if(dest.isArray()) {
          Path target = dest.path().withChild(s + cnt);
          if(!Record::db.add(
              target,
              src.value()
          )) {
            throw janosh_exception() << record_info({"add failed", target});
          }
        } else if(dest.isObject()) {
          Path target = dest.path().withChild(src.path().name());
          if(!Record::db.add(
              target,
              src.value()
          )) {
            throw janosh_exception() << record_info({"add failed",target});
          }
        }
      }
    }

    setContainerSize(dest, s + cnt);
    return cnt;
  }

  size_t Janosh::copy(Record& src, Record& dest) {
    JANOSH_TRACE({src,dest});

    src.fetch();
    dest.fetch();
    if(dest.exists() && !src.isRange()) {
      throw janosh_exception() << record_info({"destination exists", dest});
    }

    if(dest.isRange()) {
      throw janosh_exception() << record_info({"destination can't be a range", dest});
    }

    if(src == dest)
      return 0;

    if(!src.isValue() && !dest.isDirectory()) {
      throw janosh_exception() << record_info({"invalid target", dest});
    }

    if((src.isRange() || src.isDirectory()) && !dest.exists()) {
      makeDirectory(dest, src.getType());
      Record wildcard = src.path().asWildcard();
      return this->append(wildcard, dest);
    } else if (src.isValue() && dest.isValue()) {
      return this->set(dest, src.value());
    } else {
      return this->append(src, dest);
    }

    return 0;
  }

  size_t Janosh::shift(Record& src, Record& dest) {
    JANOSH_TRACE({src,dest});

    Record srcParent = src.parent();
    srcParent.fetch();

    if(!srcParent.isArray() || srcParent != dest.parent()) {
      throw janosh_exception() << record_info({"shift is limited to operate within one array", dest});
    }

    size_t parentSize = srcParent.getSize();
    size_t srcIndex = src.getIndex();
    size_t destIndex = dest.getIndex();

    if(srcIndex >= parentSize || destIndex >= parentSize) {
      throw janosh_exception() << record_info({"index out of bounds", src});
    }

    bool back = srcIndex > destIndex;
    Record forwardRec = src.clone().fetch();
    Record backRec = src.clone().fetch();

    if(back) {
      forwardRec.previous();
    } else {
      forwardRec.next();
    }

    Record tmp;

    if(src.isDirectory()) {
      tmp = makeTemp(src.getType());
      Record wildcard = src.path().asWildcard();
      this->append(wildcard, tmp);
    } else {
      tmp = makeTemp(Value::String);
      this->set(tmp, src.value());
    }

    for(int i=0; i < abs(destIndex - srcIndex); ++i) {
      replace(forwardRec, backRec);
      if(back) {
        backRec.previous();
        forwardRec.previous();
      } else {
        backRec.next();
        forwardRec.next();
      }
    }

    tmp.fetch();
    replace(tmp,dest);
    remove(tmp);
    Record tmpDir("/tmp/.");
    remove(tmpDir);
    src = dest;

    return 1;
  }

  void Janosh::setContainerSize(Record container, const size_t s) {
    JANOSH_TRACE({container}, s);

    string containerValue;
    string strContainer;
    const Value::Type& containerType = container.getType();
    char t;

    if (containerType == Value::Array)
     t = 'A';
    else if (containerType == Value::Object)
     t = 'O';
    else
     assert(false);

    const string& new_value =
       (boost::format("%c%d") % t % (s)).str();

    container.setValue(new_value);
  }

  void Janosh::changeContainerSize(Record container, const size_t by) {
    container.fetch();
    setContainerSize(container, container.getSize() + by);
  }

  size_t Janosh::load(const Path& path, const string& value) {
    return Record::db.set(path, value) ? 1 : 0;
  }

  size_t Janosh::load(js::Value& v, Path& path) {
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

  size_t Janosh::load(js::Object& obj, Path& path) {
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

  size_t Janosh::load(js::Array& array, Path& path) {
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

  bool Janosh::boundsCheck(Record p) {
    Record parent = p.parent();

    p.fetch();
    parent.fetch();

    return (parent.path().isRoot() || (!parent.isArray() || p.path().parseIndex() <= parent.getSize()));
  }



}

std::vector<size_t> sequence() {
  std::vector<size_t> v(10);
  size_t off = 5;
  std::generate(v.begin(), v.end(), [&]() {
    return ++off;
  });
  return v;
}

kyotocabinet::TreeDB janosh::Record::db;

void printUsage() {
    std::cerr << "janosh [options] <command> <paths...>" << endl
        << endl
        << "Options:" << endl
        << "  -v                enable verbose output" << endl
        << "  -j                output json format" << endl
        << "  -b                output bash format" << endl
        << "  -t                execute triggers for corresponding paths" << endl
        << "  -e <target list>  execute given targets" << endl
        << endl
        << "Commands: " << endl
        <<  "  load" << endl
        <<  "  set"  << endl
        <<  "  add" << endl
        <<  "  replace" << endl
        <<  "  append" << endl
        <<  "  dump" << endl
        <<  "  size" << endl
        <<  "  get"  << endl
        <<  "  copy" << endl
        <<  "  remove" << endl
        <<  "  shift" << endl
        <<  "  move" << endl
        <<  "  truncate" << endl
        <<  "  mkarr" << endl
        <<  "  mkobj" << endl
        <<  "  hash" << endl
        << endl;
      exit(0);
}

int main(int argc, char** argv) {
  using namespace std;
  using namespace boost;
  using namespace janosh;

  bool verbose = false;
  Janosh* janosh;
  try {
    std::vector<string> args;
    for (int i = 0; i < argc; i++) {
      args.push_back(string(argv[i]));
    }
    std::vector<char*> vc;

    int c;
    janosh::Format f = janosh::Bash;

    bool execTriggers = false;
    bool execTargets = false;

    string key;
    string value;
    string targetList;
    char* const * c_args = argv;
    optind = 1;
    while ((c = getopt(argc, c_args, "vfjbrthe:")) != -1) {
      switch (c) {
      case 'f':
        if (string(optarg) == "bash")
          f = janosh::Bash;
        else if (string(optarg) == "json")
          f = janosh::Json;
        else if (string(optarg) == "raw")
          f = janosh::Raw;
        else
          throw janosh_exception() << string_info( { "Illegal format", string(optarg) });
        break;
      case 'j':
        f = janosh::Json;
        break;
      case 'b':
        f = janosh::Bash;
        break;
      case 'r':
        f = janosh::Raw;
        break;
      case 'v':
        verbose = true;
        break;
      case 't':
        execTriggers = true;
        break;
      case 'e':
        execTargets = true;
        targetList = optarg;
        break;
      case 'h':
        printUsage();
        break;
      case ':':
        printUsage();
        break;
      case '?':
        printUsage();
        break;
      }
    }
    if (verbose)
      Logger::init(LogLevel::L_DEBUG);
    else
      Logger::init(LogLevel::L_INFO);

    janosh = new Janosh();
    janosh->setFormat(f);

    vector<std::string> vecArgs;

    if (argc >= optind + 1) {
      LOG_DEBUG_MSG("Execute command", argv[optind]);
      string strCmd = string(argv[optind]);
      if (strCmd == "get") {
        janosh->open(true);
      } else {
        janosh->open(false);
      }

      Command* cmd = janosh->cm[strCmd];

      if (!cmd) {
        throw janosh_exception() << string_info( { "Unknown command", strCmd });
      }

      vecArgs.clear();

      std::transform(args.begin() + optind + 1, args.end(), std::back_inserter(vecArgs), boost::bind(&std::string::c_str, _1));

      Command::Result r = (*cmd)(vecArgs);
      if (r.first == -1)
        throw janosh_exception() << msg_info(r.second);

      LOG_INFO_MSG(r.second, r.first);
    } else if (!execTargets) {
      throw janosh_exception() << msg_info("missing command");
    }

    janosh->close();

    if (execTriggers) {
      LOG_DEBUG("Triggers");
      Command* t = janosh->cm["trigger"];
      vector<string> vecTriggers;

      for (size_t i = 0; i < vecArgs.size(); i += 2) {
        LOG_DEBUG_MSG("Execute triggers", vecArgs[i].c_str());
        vecTriggers.push_back(vecArgs[i].c_str());
      }
      (*t)(vecTriggers);
    }

    if (execTargets) {
      Command* t = janosh->cm["target"];
      vector<string> vecTargets;
      tokenizer<char_separator<char> > tok(targetList, char_separator<char>(","));
      BOOST_FOREACH (const string& t, tok) {
        vecTargets.push_back(t);
      }

      (*t)(vecTargets);
    }

  } catch (janosh_exception& ex) {
    janosh->printException(ex);
    return 1;
  } catch (std::exception& ex) {
    janosh->printException(ex);
    return 1;
  }
  janosh->close();
  return 0;
}

