#include "record.hpp"

namespace janosh {
  typedef  boost::shared_ptr<kc::DB::Cursor> Base;

  void Record::init(const Path path) {
    if(path.isWildcard()) {
      if(this->jump(path.asDirectory())) {
        this->doesExist = true;
        this->pathObj = path;
        assert(this->readValue());
        getCursorPtr()->step();
      } else {
        this->doesExist = false;
      }
    } else if(path.isRoot()) {
      getCursorPtr()->jump("/!");
      this->pathObj = path;
      assert(this->readValue());
      this->doesExist = true;
    } else {
      if(this->jump(path)) {
        this->doesExist = true;
        assert(this->readValue());
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
    return Record(this->path());
  }

  Record::Record(const Path& path) :
    Base(Record::db.cursor()),
    pathObj(path),
    doesExist(false)
  {}

  Record::Record() : Base(),
    doesExist(false){
  }

  kc::DB::Cursor* Record::getCursorPtr() {
    return Base::operator->();
  }

  const Value::Type Record::getType()  const {
    if(this->isDirectory()) {
      assert(this->hasData());
      return value().getType();
    }

    return Value::String;
  }

  const size_t Record::getSize() const {
    assert(hasData());
    return value().getSize();
  }

  const size_t Record::getIndex() const {
    assert(isInitialized());
    return path().parseIndex();
  }

  bool Record::remove() {
    assert(isInitialized());
    assert(getCursorPtr()->remove());
    this->clear();
    readPath();
    return true;
  }

  bool Record::setValue(const string& v) {
    assert(isInitialized());
    return getCursorPtr()->set_value_str(v);
  }

  bool Record::get(string& k, string& v) {
    assert(isInitialized());
    return getCursorPtr()->get(&k,&v);
  }

  bool Record::jump(const Path& p) {
    assert(isInitialized());
    if(getCursorPtr()->jump(p.key())) {
      readPath();
      return this->path() == p;
    } else
      return false;
  }

  bool Record::jump_back(const Path& p) {
    assert(isInitialized());
    return getCursorPtr()->jump_back(p.key());
  }

  bool Record::step() {
    assert(isInitialized());
    bool r = getCursorPtr()->step();
    if(r) {
      this->clear();
      readPath();
    }
    return r;
  }

  bool Record::step_back() {
    assert(isInitialized());
    bool r = getCursorPtr()->step_back();
    if(r) {
      this->clear();
      readPath();
    }
    return r;
  }

  bool Record::next() {
    assert(isInitialized());
    bool success;

    if(this->isDirectory()) {
      size_t s = this->getSize();
      success = this->step();

      for(size_t i = 0; success && i < s; ++i) {
        success &= this->step();
      }
    } else {
      success = this->step();
    }

    return success;
  }

  bool Record::previous() {
    assert(isInitialized());
    Record parent = this->parent();

    Record prev;
    Record prevParent;

    do {
      if(!this->step_back())
        return false;
      prev = *this;
      prevParent = prev.parent();
    } while(prevParent != parent && parent.isAncestorOf(prev));

    this->clear();
    readPath();

    return true;
  }

  const bool Record::isAncestorOf(const Record& other) const {
    return this->path().above(other.path());
  }

  void Record::clear() {
    this->pathObj.reset();
    this->valueObj.reset();
  }

  Record Record::parent() const {
    return Record(this->path().parent());
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
    assert(isInitialized());
    return this->valueObj;
  }

  bool Record::readValue() {
    assert(isInitialized());
    assert(!empty());
    string v;
    bool s = getCursorPtr()->get_value(&v);

    if(path().isDirectory()) {
      valueObj = Value(v, true);
    } else if(path().isWildcard()) {
      valueObj = Value(v, Value::Range);
    } else {
      valueObj = Value(v, false);
    }

    return s;
  }

  bool Record::readPath() {
    assert(isInitialized());
    string k;
    bool s = getCursorPtr()->get_key(&k);
    pathObj = k;
    return s;
  }

  bool Record::read() {
    return readPath() && readValue();
  }

  Record Record::fetch() {
    assert(isInitialized());
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
}

