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
    if(this->path().isDirectory()) {
      return value().getType();
    } else if(this->path().isWildcard()) {
      return Value::Wildcard;
    } else {
      return Value::String;
    }
  }

  const size_t Record::getSize() const {
    return value().getSize();
  }

  const size_t Record::getIndex() const {
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
    this->valueObj = Value();
  }

  Record Record::parent() const {
    return Record(this->path().parent());
  }

  const bool Record::isArray() const {
    return this->getType() == Value::Array;
  }

  const bool Record::isDirectory() const {
    return this->path().isDirectory();
  }

  const bool Record::isWildcard() const {
    return this->getType() == Value::Wildcard;
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

  bool Record::readPath() {
    assert(isInitialized());
    string k;
    bool s = getCursorPtr()->get_key(&k);
    pathObj = k;
    return s;
  }

  bool Record::readValue() {
    string v;
    assert(isInitialized());
    const Path& p = this->path();

    if(p.isWildcard()) {
      Record wild = p.asDirectory();
      if(!wild.getCursorPtr()->get_value(&v)) {
        this->valueObj = Value();
      } else {
        size_t s = boost::lexical_cast<size_t>(v.substr(1));
        this->valueObj = Wildcard(&wild, v, s);
      }
    } else {
      if(this->getCursorPtr()->get_value(&v)) {
        this->valueObj = Value();
      }
      if(p.isDirectory()) {
        char c = v.at(0);
        size_t s = boost::lexical_cast<size_t>(v.substr(1));

        if(c == 'A') {
          this->valueObj = Array(this, v, s);
        } else if(c == 'O') {
          this->valueObj = Object(this, v, s);
        } else {
          assert(!"Unknown directory descriptor");
          this->valueObj = Value();
        }
      } else {
        this->valueObj = String(this, v);
      }
    }

    return this->valueObj.getType() != Value::Empty;
  }

  bool Record::read() {
    return readPath() && readValue();
  }

  Record& Record::fetch() {
    assert(isInitialized());
    init(this->path());
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
}
