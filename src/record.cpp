#include "record.hpp"
#include "exception.hpp"
#include "tracker.hpp"
#include "logger.hpp"

namespace janosh {

  void Record::init(Path path) {
    if(path.isWildcard()) {
      if(this->jump(path.asDirectory())) {
        this->doesExist = true;
        this->pathObj = path;
        if(!this->readValue())
          throw record_exception() << path_info({"can't initialize", path});

    //    getCursorPtr()->step();
      } else {
        this->doesExist = false;
      }
    } else if(path.isRoot()) {
      getCursorPtr()->jump("/!");
      this->pathObj = path;
      if(!this->readValue())
        throw record_exception() << path_info({"can't initialize", path});

      this->doesExist = true;
    } else {
      if(this->jump(path)) {
        this->doesExist = true;
        if(!this->readValue())
          throw record_exception() << path_info({"can't initialize", path});
      } else {
        this->doesExist = false;
      }
      this->pathObj = path;
    }
  }

  //exact copy referring to the same Cursor*
  Record::Record(const Record& other) :
    Base(other),
    pathObj(other.pathObj),
    valueObj(other.valueObj),
    doesExist(other.doesExist) {}

  //clone the cursor position not the Cursor*
  Record Record::clone() {
    return RecordPool::get(this->path());
  }

  Record::Record(const Path& path) :
    Base(Record::getDB()->cursor()),
    pathObj(path),
    doesExist(false)
  {}

  Record::Record() : Base(),
    doesExist(false){
  }

  void Record::makeDB(const string& host, const int32_t& port) {
    auto it = Record::db.find(std::this_thread::get_id());
    if(it != Record::db.end())
      throw janosh_exception() << msg_info("DB already initialized");
    kyototycoon::RemoteDB* rdb = new kyototycoon::RemoteDB();
    rdb->open(host, port);
    Record::db[std::this_thread::get_id()] = rdb;

  }
  kyototycoon::RemoteDB* Record::getDB() {
    auto it = Record::db.find(std::this_thread::get_id());
    if(it == Record::db.end())
      throw janosh_exception() << msg_info("DB not initialized");

    return (*it).second;
  }

  void Record::destroyDB() {
    auto it = Record::db.find(std::this_thread::get_id());
    if(it == Record::db.end())
      throw janosh_exception() << msg_info("DB not initialized");

    kyototycoon::RemoteDB* rdb = (*it).second;
    Record::db.erase(it);
    rdb->close();
    delete rdb;
  }

  janosh::Cursor* Record::getCursorPtr() {
    return Base::get();
  }

  const Value::Type Record::getType()  const {
    if(this->path().isDirectory()) {
      return value().getType();
    } else if(this->path().isWildcard()) {
      return Value::Range;
    } else {
      return Value::String;
    }
  }

  const size_t Record::getSize() const {
    if(!this->hasData())
      throw record_exception() << path_info({"uninitialized record", this->pathObj});

    return value().getSize();
  }

  const size_t Record::getIndex() const {
    if(!isInitialized())
      throw record_exception() << path_info({"uninitialized record", this->pathObj});
    return path().parseIndex();
  }

  void Record::remove() {
    LOG_DEBUG_MSG("remove", "\"" + this->pathObj.pretty() + "\"");

    if(!isInitialized())
      throw record_exception() << path_info({"uninitialized record", this->pathObj});

    if(!getCursorPtr()->remove())
      throw record_exception() << path_info({"failed to remove record", this->pathObj});

    this->clear();
    readPath();
  }

  void Record::setPath(const string& p) {
    this->pathObj = p;
    this->doesExist = false;
  }

  bool Record::setValue(const string& v) {
    if(!isInitialized())
      throw record_exception() << path_info({"uninitialized record", this->pathObj});

    return getCursorPtr()->set_value_str(v);
  }

  bool Record::get(string& k, string& v) {
    if(!isInitialized())
      throw record_exception() << path_info({"uninitialized record", this->pathObj});

    return getCursorPtr()->get(&k,&v);
  }

  bool Record::jump(const Path& p) {
    if(!isInitialized())
      throw record_exception() << path_info({"uninitialized record", this->pathObj});

    if(getCursorPtr()->jump(p.key())) {
      readPath();
      return this->path() == p;
    } else
      return false;
  }

  bool Record::jump_back(const Path& p) {
    if(!isInitialized())
      throw record_exception() << path_info({"uninitialized record", this->pathObj});

    return getCursorPtr()->jump_back(p.key());
  }

  bool Record::step() {
    if(!isInitialized())
      throw record_exception() << path_info({"uninitialized record", this->pathObj});

    bool r = getCursorPtr()->step();
    if(r) {
      this->clear();
      read();
    }
    return r;
  }

  bool Record::step_back() {
    if(!isInitialized())
      throw record_exception() << path_info({"uninitialized record", this->pathObj});

    bool r = getCursorPtr()->step_back();
    if(r) {
      this->clear();
      read();
    }
    return r;
  }

  void Record::nextMember() {
    if(!isInitialized())
      throw record_exception() << path_info({"uninitialized record", this->pathObj});

    if(this->isDirectory()) {
      Path parent = this->path();
      size_t s = this->getSize();
      if(!this->step())
        throw record_exception() << path_info({"step failed", this->pathObj});

      for(size_t i = 0; i < s; ++i) {
        this->fetch();
        if(this->path().parent() != parent) {
          while(this->path().parent() != parent) {
            if(!this->step())
              throw record_exception() << path_info({"step failed", this->pathObj});
          }
        } else {
          if(!this->step())
            throw record_exception() << path_info({"step failed", this->pathObj});
        }
      }
    } else {
      if(!this->step())
        throw record_exception() << path_info({"step failed", this->pathObj});
    }
  }

  void Record::next() {
    if(!isInitialized())
      throw record_exception() << path_info({"uninitialized record", this->pathObj});

    if(this->isDirectory()) {
      size_t s = this->getSize();
      if(!this->step())
        throw record_exception() << path_info({"step failed", this->pathObj});

      for(size_t i = 0; i < s; ++i) {
        if(!this->step())
          throw record_exception() << path_info({"step failed", this->pathObj});
      }
    } else {
      if(!this->step())
        throw record_exception() << path_info({"step failed", this->pathObj});
    }
  }

  void Record::previous() {
    if(!isInitialized())
            throw record_exception() << path_info({"uninitialized record", this->pathObj});

    Record parent = this->parent();

    Record prev;
    Record prevParent;

    do {
      if(!this->step_back())
        throw record_exception() << path_info({"step back failed", this->pathObj});
      prev = *this;
      prevParent = prev.parent();
    } while(prevParent != parent && parent.isAncestorOf(prev));

    this->clear();
    readPath();
  }

  const bool Record::isAncestorOf(const Record& other) const {
    return this->path().above(other.path());
  }

  void Record::clear() {
    this->pathObj.reset();
    this->valueObj.reset();
  }

  Record Record::parent() const {
    return RecordPool::get(this->path().parent());
  }

  vector<Record> Record::getParents() const {
    vector<Record> parents;
    if(this->path().isRoot())
      return parents;

    Record parent = this->parent();
    while(true) {
      parents.push_back(parent);
      if(parent.path().isRoot())
        break;

      parent = parent.parent();
    }

    std::reverse(parents.begin(), parents.end());

    return parents;
  }

  const bool Record::isArray() const {
    return this->isDirectory() && this->getType() == Value::Array;
  }

  const bool Record::isDirectory() const {
    return this->path().isDirectory();
  }

  const bool Record::isRange() const {
    return this->getType() == Value::Range;
  }

  const bool Record::isValue() const {
    return !this->path().isDirectory() || this->getType() == Value::String;
  }

  const bool Record::isObject() const {
    return this->getType() == Value::Object;
  }

  const bool Record::isInitialized() const {
    return (*this) != NULL;
  }

  const bool Record::hasData() const {
    return valueObj.isInitialized();
  }

  const bool Record::exists() const {
    return this->doesExist;
  }

  const bool Record::empty() const {
    return path().isEmpty();
  }

  const Path& Record::path() const {
    return this->pathObj;
  }

  const Value& Record::value() const {
    if(!isInitialized())
            throw record_exception() << path_info({"uninitialized record", this->pathObj});

    return this->valueObj;
  }

  bool Record::readValue() {
    if(!isInitialized())
            throw record_exception() << path_info({"uninitialized record", this->pathObj});

    if(empty())
      throw record_exception() << path_info({"no value found", this->pathObj});

    string v;
    bool s = getCursorPtr()->get_value(&v);
    Tracker::getInstancePerThread()->update(path().pretty(), v, Tracker::READ);
    if(path().isDirectory()) {
      valueObj = Value(v, true);
    } else if(path().isWildcard()) {
      valueObj = Value(v, Value::Range);
    } else {
      if(v.at(0) == 'n')
        valueObj = Value(v.substr(1), Value::Number);
      if(v.at(0) == 'b')
        valueObj = Value(v.substr(1), Value::Boolean);
      if(v.at(0) == 's')
        valueObj = Value(v.substr(1), Value::String);
    }

    return s;
  }

  bool Record::readPath() {
    if(!isInitialized())
            throw record_exception() << path_info({"uninitialized record", this->pathObj});

    string k;
    bool s = getCursorPtr()->get_key(&k);
    pathObj = k;

    return s;
  }

  bool Record::read() {
    if(!isInitialized())
             throw record_exception() << path_info({"uninitialized record", this->pathObj});

     string k;
     string v;
     bool s = getCursorPtr()->get(&k, &v);
     pathObj = k;
     Tracker::getInstancePerThread()->update(path().pretty(), v, Tracker::READ);
     if(path().isDirectory()) {
       valueObj = Value(v, true);
     } else if(path().isWildcard()) {
       valueObj = Value(v, Value::Range);
     } else {
       if(v.at(0) == 'n')
         valueObj = Value(v.substr(1), Value::Number);
       if(v.at(0) == 'b')
         valueObj = Value(v.substr(1), Value::Boolean);
       if(v.at(0) == 's')
         valueObj = Value(v.substr(1), Value::String);
     }
     return s;
  }

  Record& Record::fetch() {
    if(!isInitialized())
      throw record_exception() << path_info({"uninitialized record", this->pathObj});

    if(!hasData()) {
      init(this->path());
    }
    return *this;
  }

  bool Record::operator==(const Record& other) const {
    return this->path() == other.path();
  }

  bool Record::operator!=(const Record& other) const {
    return !(this->path() == other.path());
  }

  std::ostream& operator<< (std::ostream& os, const janosh::Path& p) {
      os << p.key();
      return os;
  }

  std::ostream& operator<< (std::ostream& os, const janosh::Value& v) {
      os << v.str();
      return os;
  }

  std::ostream& operator<< (std::ostream& os, const janosh::Record& r) {
      os << "path=" << r.path().pretty()
          << " exists=" << (r.exists() ? "true" : "false")
          << " value=" <<  (r.isInitialized() && r.value().isInitialized() ? r.value().str() : "N/A");

      return os;
  }

  std::mutex RecordPool::mutex_;
  std::vector<Record> RecordPool::pool_;
}

