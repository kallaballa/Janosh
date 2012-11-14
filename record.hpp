#ifndef _JANOSH_RECORD_HPP
#define _JANOSH_RECORD_HPP

#include <sstream>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <boost/smart_ptr.hpp>
#include <kcpolydb.h>
#include "Logger.hpp"
#include "value.hpp"

namespace janosh {
  namespace kc = kyotocabinet;
  typedef kc::DB::Cursor Cursor;

  class Record : private boost::shared_ptr<Cursor> {
    Path pathObj;
    Value valueObj;
    bool doesExist;

    void init(const Path path);
  public:
    static kc::TreeDB db;

    //exact copy referring to the same Cursor*
    Record(const Record& other);
    Record clone();
    Record(const Path& path);
    Record();

    kc::DB::Cursor* getCursorPtr();
    const Value::Type getType()  const;
    const size_t getSize() const;
    const size_t getIndex() const;

    bool get(string& k, string& v);
    bool setValue(const string& v);

    void clear();

    bool jump(const Path& p);
    bool jump_back(const Path& p);
    bool step();
    bool step_back();
    bool previous();
    bool remove();

    const Path& path() const;
    const Value& value() const;
    Record parent() const;


    const bool isAncestorOf(const Record& other) const;
    const bool isArray() const;
    const bool isDirectory() const;
    const bool isWildcard() const;
    const bool isValue() const;
    const bool isObject() const;
    const bool isInitialized() const;
    const bool hasData() const;
    const bool exists() const;
    const bool empty() const;

    Record& fetch();
    bool readValue();
    bool readPath();
    bool read();


    bool operator==(const Record& other) const;
    bool operator!=(const Record& other) const;
  };

  static Value readValue(Record& rec) {
    string v;

    const Path& p = rec.path();

    if(p.isWildcard()) {
      Record wild = p.asDirectory();
      if(!wild.getCursorPtr()->get_value(&v))
        return Value();
      size_t s = boost::lexical_cast<size_t>(v.substr(1));

      return Wildcard(&wild, v, s);
    } else {
      if(rec.getCursorPtr()->get_value(&v))
        return Value();

      if(p.isDirectory()) {
        char c = v.at(0);
        size_t s = boost::lexical_cast<size_t>(v.substr(1));

        if(c == 'A') {
          return Array(&rec, v, s);
        } else if(c == 'O') {
          return Object(&rec, v, s);
        } else {
          assert(!"Unknown directory descriptor");
          return Value();
        }
      } else {
        return String(&rec, v);
      }
    }
  }


}

#endif
