#ifndef _JANOSH_DBPATH_HPP
#define _JANOSH_DBPATH_HPP

#include <memory>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>

#include "path.hpp"
#include "value.hpp"
#include "logger.hpp"

namespace janosh {
  class RecordPool;

  class Record : public std::shared_ptr<janosh::Cursor> {
    friend class RecordPool;
    Path pathObj;
    Value valueObj;
    bool doesExist;
    void init(Path path);
    static kyototycoon::TimedDB* db;
    //exact copy referring to the same Cursor*
    Record(const Path& path);
    janosh::Cursor* getCursorPtr();
  public:
    typedef  std::shared_ptr<janosh::Cursor> Base;

    Record();
    Record(const Record& other);
    Record clone();

    static kyotocabinet::PolyDB* getDB();
    static void setDB(kyototycoon::TimedDB* db);

    const Value::Type getType()  const;
    const size_t getSize() const;
    const size_t getIndex() const;

    bool get(string& k, string& v);
    void setPath(const string& p);
    bool setValue(const string& v);

    void clear();

    bool jump(const Path& p);
    bool jump_back(const Path& p);
    bool step();
    bool step_back();
    void next();
    void nextMember();
    void previous();
    void remove();

    const Path& path() const;
    const Value& value() const;
    Record parent() const;
    vector<Record> getParents() const;

    const bool isAncestorOf(const Record& other) const;
    const bool isArray() const;
    const bool isDirectory() const;
    const bool isRange() const;
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

  std::ostream& operator<< (std::ostream& os, const janosh::Path& p);
  std::ostream& operator<< (std::ostream& os, const janosh::Value& v);
  std::ostream& operator<< (std::ostream& os, const janosh::Record& r);

  class RecordPool {
    static std::mutex mutex_;
    static std::vector<Record> pool_;
public:
    static Record get(const string& path) {
      //std::unique_lock<std::mutex>(mutex_);
      return Record(path);
      //FIXME pool records;
//      if(pool_.empty()) {
//        Record r(path);
//        pool_.push_back(r);
//        return r;
//      } else {
//        for(auto& r : pool_) {
//          if(static_cast<Record::Base*>(&r)->use_count() == 1) {
//            r.setPath(path);
//            return r;
//          }
//        }
//
//        Record r(path);
//        pool_.push_back(r);
//        return r;
//      }
    }
  };
}
#endif
