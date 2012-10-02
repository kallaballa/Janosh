#ifndef _JANOSH_DBPATH_HPP
#define _JANOSH_DBPATH_HPP

#include <stack>
#include <sstream>
#include <iostream>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <boost/smart_ptr.hpp>
#include <kcpolydb.h>
#include "Logger.hpp"

using std::string;

namespace janosh {
  namespace kc = kyotocabinet;
  enum EntryType {
    Array,
    Object,
    Value
  };


  class DBPath {
    string keyStr;
    string valueStr;

    bool hasKey;
    bool hasValue;

    std::vector<string> components;
    bool container;
    bool wildcard;
    bool doesExist;

    string compilePathString() const {
      std::stringstream ss;

      for (auto it = this->components.begin(); it != this->components.end(); ++it) {
        ss << *it;
      }
      return ss.str();
    }
  public:
    static kc::TreeDB db;

    class Cursor : private boost::shared_ptr<kc::DB::Cursor> {
      typedef  boost::shared_ptr<kc::DB::Cursor> parent;

      kc::DB::Cursor* getCursorPtr() {
        return parent::operator->();
      }
    public:
      Cursor() :
        parent()
      {};

      Cursor(kc::DB::Cursor* cur) : parent(cur) {
      }

      Cursor(const DBPath& path) :
        parent(DBPath::db.cursor()) {

        if(path.isWildcard()) {
          parent::operator->()->jump(path.basePath());
          parent::operator->()->step();
        } else {
          parent::operator->()->jump(path.key());
        }
      }

      bool isValid() {
        return (*this) != NULL;
      }

      bool remove() {
        assert(isValid());
        return getCursorPtr()->remove();
      }

      bool setValue(const string& v) {
        assert(isValid());
        return getCursorPtr()->set_value_str(v);
      }

      bool get(string& k, string& v) {
        assert(isValid());
        return getCursorPtr()->get(&k,&v);
      }

      bool jump(const string& k) {
        assert(isValid());
        return getCursorPtr()->jump(k);
      }

      bool getValue(string& v) {
        assert(isValid());
        return getCursorPtr()->get_value(&v);
      }

      bool step() {
        assert(isValid());
        return getCursorPtr()->step();
      }

      bool step_back() {
        assert(isValid());
        return getCursorPtr()->step_back();
      }

      bool next() {
        assert(isValid());
        DBPath p(*this);

        if(p.isContainer()) {
          bool success = this->step();
          size_t s = p.getSize();

          for(size_t i = 0; success && i < s; ++i) {
            success &= this->step();
          }

          LOG_DEBUG_MSG("next", p.key() + "->" + DBPath(*this).key());
          return success;
        } else {
          bool success = this->step();
          LOG_DEBUG_MSG("next", p.key() + "->" + DBPath(*this).key());
          return success;
        }
      }

      bool previous() {
        assert(isValid());
        DBPath p(*this);
        DBPath parent = p.parent();

        DBPath prev;
        DBPath prevParent;

        do {
          if(!this->step_back())
            return false;
          prev = *this;
          DBPath prevParent = prev.parent();
        } while(prevParent != parent && parent.above(prev));
        return true;
      }
    };

    DBPath(const string& strPath) :
        hasKey(false),
        hasValue(false),
        container(false),
        wildcard(false),
        doesExist(false) {
      update(strPath);
    }

    DBPath(Cursor& cur):
        hasKey(false),
        hasValue(false),
        container(false),
        wildcard(false),
        doesExist(false) {
      read(cur);
    }

    DBPath() :
      hasKey(false),
      hasValue(false),
      container(false),
      wildcard(false),
      doesExist(false){
    }

    DBPath(const DBPath& other) {
      keyStr = other.keyStr;
      valueStr = other.valueStr;
      hasKey = other.hasKey;
      hasValue = other.hasValue;
      container = other.container;
      wildcard = other.wildcard;
      doesExist = other.doesExist;
      components = other.components;
    }

    void clear() {
      this->keyStr.clear();
      this->valueStr.clear();
      this->hasKey=false;
      this->hasValue=false;
      this->container=false;
      this->wildcard=false;
      this->container=false;
      this->components.clear();
    }

    void update(const string& p) {
      using namespace boost;
      if(p.empty()) {
        this->clear();
        return;
      }

      if(p.at(0) != '/') {
        LOG_ERR_MSG("Illegal Path", p);
        exit(1);
      }

      char_separator<char> ssep("[/", "", boost::keep_empty_tokens);
      tokenizer<char_separator<char> > tokComponents(p, ssep);
      this->components.clear();
      tokComponents.begin();
      if(tokComponents.begin() != tokComponents.end()) {
        auto it = tokComponents.begin();
        it++;
        for(; it != tokComponents.end(); it++) {
          const string& c = *it;
          if(c.empty()) {
            LOG_ERR_MSG("Illegal Path", p);
            exit(1);
          }
          this->components.push_back('/' + c);
        }

        this->keyStr = p;
        this->hasKey = true;
        this->container = !this->components.empty() && this->components.back() == "/.";
        this->wildcard =  !this->components.empty() && this->components.back() == "/*";
      } else {
        clear();
      }
    }

    const bool isContainer() const {
      return this->container;
    }

    const bool isWildcard() const {
      return this->wildcard;
    }

    const bool isComplete() const {
      return hasValue;
    }

    const bool exists() const {
      return this->doesExist;
    }

    const bool empty() const {
      return !hasKey;
    }

    const string& val() const {
      assert(isComplete());
      return this->valueStr;
    }

    DBPath prune(Cursor cur = Cursor()) {
      if(!isComplete())
        read(cur);

      return *this;
    }

    Cursor getCursor(Cursor cur = Cursor()) {
      if(this->empty()) {
        return Cursor();
      } else if(!cur.isValid()) {
        cur = Cursor(DBPath::db.cursor());
      }

      if(this->isWildcard()) {
        cur.jump(this->basePath());
        cur.step();
      } else {
        cur.jump(this->key());
      }

      return cur;
    }

    DBPath makeDirectory() {
      return DBPath(this->basePath() + "/.");
    }

    DBPath makeWildcard() {
      return DBPath(this->basePath() + "/*");
    }

    DBPath makeChild(const string& name) {
      return DBPath(this->basePath() + '/' + name);
    }

    DBPath makeChild(const size_t& i) {
      return DBPath(this->basePath() + '/' + boost::lexical_cast<string>(i));
    }

    Cursor read(Cursor cur = Cursor()) {
      if(this->empty()) {
        if(!cur.isValid()) {
          LOG_ERR("Can't read key from empty cursor");
          exit(1);
        }
        string k;
        if(cur.get(k,valueStr)) {
          this->hasValue = true;
          this->doesExist = true;
          update(k);
        } else {
          this->hasKey = false;
          this->hasValue = false;
          this->doesExist = false;
          return Cursor();
        }
      } else {
        if (!cur.isValid()) {
          cur = Cursor(DBPath::db.cursor());
          string k;
          if (!cur.jump(this->keyStr)) {
            this->doesExist = false;
            return Cursor();
          } else {
            cur.get(k, this->valueStr);
            this->hasValue = true;
            this->doesExist = true;
            update(k);
          }
        } else if (!cur.getValue(this->valueStr)) {
          this->hasValue = true;
          this->doesExist = false;
          return Cursor();
        } else {
          this->doesExist = true;
        }
      }

      return cur;
    }

    bool isRoot() const {
      return this->key() == "/.";
    }

    const string& key() const {
      return this->keyStr;
    }

    bool operator<(const DBPath& other) const {
      return this->key() < other.key();
    }

    bool operator==(const DBPath& other) const {
      return this->key() == other.key();
    }

    bool operator!=(const DBPath& other) const {
      return this->key() != other.key();
    }

    void pushMember(const string& name) {
      components.push_back((boost::format("/%s") % name).str());
      update(compilePathString());
    }

    void pushIndex(const size_t& index) {
      components.push_back((boost::format("/%d") % index).str());
      update(compilePathString());
    }

    void pop(bool doUpdate=false) {
      components.erase(components.end() - 1);
      if(doUpdate) update(compilePathString());
    }

    string basePath() const {
      if(isContainer() || isWildcard()) {
        return this->keyStr.substr(0, this->keyStr.size() - 2);
      } else {
        return this->key();
      }
    }

    string name() const {
      size_t d = 1;

      if(isContainer())
        ++d;

      if(components.size() >= d)
        return *(components.end() - d);
      else
        return "";
    }

    size_t parseIndex() const {
        return boost::lexical_cast<size_t>(this->name().substr(1));
    }

    DBPath parent() const {
      DBPath parent(this->basePath());
      parent.pop(false);
      parent.pushMember(".");

      return parent;
    }

    string parentName() const {
      size_t d = 2;
      if(isContainer())
        ++d;

      if(components.size() >= d)
        return *(components.end() - d);
      else
        return "";
    }

    string root() const {
      return components.front();
    }

    const EntryType getType()  const {
      assert(isComplete());

      if(this->isContainer()) {
        char c = valueStr.at(0);
        if(c == 'A') {
          return Array;
        } else if(c == 'O') {
          return Object;
        } else {
          assert(!"Unknown container descriptor");
        }
      }

      return Value;
    }

    const size_t getSize() const {
      assert(isComplete());

      if(this->isContainer()) {
        return boost::lexical_cast<size_t>(valueStr.substr(1));
      } else {
        return 0;
      }


    }

    bool above(const DBPath& other) const {
      if(other.components.size() >= this->components.size() ) {
        size_t depth = this->components.size();
        if(this->isContainer()) {
          depth--;
        }

        size_t i;
        for(i = 0; i < depth; ++i) {

          const string& tc = this->components[i];
          const string& oc = other.components[i];

          if(oc != tc) {
            return false;
          }
        }

        return true;
      }

      return false;
    }
  };

  typedef DBPath::Cursor Cursor;
}


#endif
