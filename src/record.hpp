#ifndef _JANOSH_DBPATH_HPP
#define _JANOSH_DBPATH_HPP

#include <memory>
#include <ktremotedb.h>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>

#include "path.hpp"
#include "value.hpp"
#include "logger.hpp"

namespace janosh {
  typedef  std::shared_ptr<janosh::Cursor> Base;

  class Record : private Base {
    Path pathObj;
    Value valueObj;
    bool doesExist;
    static std::mutex dbMutex;
    void init(Path path);
    static std::map<std::thread::id,kyototycoon::RemoteDB*> db;
  public:

    //exact copy referring to the same Cursor*
    Record(const Record& other);
    Record(const Path& path);
    Record clone();
    Record();

    static kyototycoon::RemoteDB* getDB();
    static void makeDB();
    static void destroyAllDB();
    janosh::Cursor* getCursorPtr();
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
}
#endif
