#include "janosh.hpp"
#include "commands.hpp"
#include "tcp_server.hpp"
#include "tcp_client.hpp"
#include "janosh_thread.hpp"
#include "exception.hpp"
#include "tracker.hpp"
#include "logger.hpp"
#include "json.hpp"
#include "bash.hpp"
#include "raw.hpp"
#include "exithandler.hpp"
#include "lua_script.hpp"
#include "message_queue.hpp"

#include <stack>
#include <thread>
#include <chrono>
#include <boost/program_options.hpp>
#include <boost/functional/hash.hpp>
#include <boost/tokenizer.hpp>
#include <boost/token_functions.hpp>
#include <boost/format.hpp>
#include <boost/bind.hpp>
#include "json_spirit/json_spirit.h"

using std::string;
using std::endl;
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

  Janosh::Janosh() :
		settings_(),
        cm_(makeCommandMap(this)), open_(false) {
 }

  Janosh::~Janosh() {
  }

  void announceOperation(const string& str, const string& value, const Tracker::Operation& op) {
    Tracker::getInstancePerThread()->update(str, value, op);
  }

  void announceOperation(const string& str, const char* value, const Tracker::Operation& op) {
    Tracker::getInstancePerThread()->update(str, value, op);
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
      mode = kc::PolyDB::OTRYLOCK | kc::PolyDB::OREADER | kc::PolyDB::OWRITER | kc::PolyDB::OCREATE;
    if(!Record::db.open(settings_.databaseFile.string(),  mode)) {
			LOG_FATAL_MSG("open error: " + settings_.databaseFile.string(), Record::db.error().message());
    }
    ExitHandler::getInstance()->addExitFunc([&](){this->close(); });
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

  bool Janosh::beginTransaction() {
    return Record::db.begin_transaction();
  }

  bool Janosh::beginTransactionTry() {
    return Record::db.begin_transaction_try();
  }

  void Janosh::endTransaction(bool commit) {
    Record::db.end_transaction(commit);
  }

  void Janosh::publish(const string& key, const string& op, const char* value) {
    MessageQueue::getInstance()->publish(key,op, value);
  }

  void Janosh::publish(const string& key, const string& op, const string& value) {
    MessageQueue::getInstance()->publish(key,op, value.c_str());
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
  size_t Janosh::get(vector<Record> recs, std::ostream& out) {
    PrintVisitor* vis = NULL;
    switch (this->getFormat()) {
    case Json:
      vis = new JsonPrintVisitor(out);
      break;
    case Bash:
      vis = new BashPrintVisitor(out);
      break;
    case Raw:
      vis = new RawPrintVisitor(out);
      break;
    }
    vis->begin();

    size_t cnt = 1;

    if(recs.size() > 1 && this->getFormat() == Json)
      out << "{" << std::endl;

    bool first = true;
    for (Record& rec : recs) {
      JANOSH_TRACE( { rec });
      rec.fetch();

      LOG_DEBUG_MSG("get", rec.path().pretty());

      if (!rec.exists()) {
        throw janosh_exception() << record_info( { "Path not found", rec });
      }

      if(recs.size() > 1 && this->getFormat() == Json && !first)
        out << "," << std::endl;

      if (rec.isDirectory()) {
        recurseDirectory(rec, vis, (recs.size() > 1 ? Value::Object : Value::Array), out);
      } else {
        recurseValue(rec, vis, (recs.size() > 1 ? Value::Object : Value::Array), out);
      }
      first = false;
    }

    if(recs.size() > 1 && this->getFormat() == Json)
      out << "}" << std::endl;

    vis->close();
    delete vis;
    return cnt;
  }

  size_t Janosh::recurseValue(Record& travRoot, PrintVisitor* vis, Value::Type rootType, ostream& out) {
    JANOSH_TRACE( { travRoot });

    vector<Record> parents = travRoot.getParents();
    parents.push_back(travRoot);

    std::stack<std::pair<const Component, const Value::Type> > hierachy;
    size_t cnt = 0;
    Path last;
    Record rec;
    for (size_t i = 0; i < parents.size(); ++i) {
      rec = parents[i];
      rec.fetch();
      const Path& path = rec.path();
      const Value& value = rec.value();
      const Value::Type& t = rec.getType();
      const Path& parent = path.parent();

      const Component& name = path.name();
      const Component& parentName = parent.name();

      if (!hierachy.empty()) {
        if (!last.above(path) && ((!last.isDirectory() && parentName != last.parentName()) || (last.isDirectory() && parentName != last.name()))) {
          while (!hierachy.empty() && hierachy.top().first != parentName) {
            if (hierachy.top().second == Value::Array) {
              vis->endArray(path);
            } else if (hierachy.top().second == Value::Object) {
              vis->endObject(path);
            }
            hierachy.pop();
          }
        }
      }

      if (t == Value::Array) {
        Value::Type parentType;
        if (hierachy.empty())
          parentType = rootType;
        else
          parentType = hierachy.top().second;

        hierachy.push( { name, Value::Array });
        vis->beginArray(path, parentType == Value::Array, last.isEmpty() || last == parent);
      } else if (t == Value::Object) {
        Value::Type parentType;
        if (hierachy.empty())
          parentType = rootType;
        else
          parentType = hierachy.top().second;

        if(!name.pretty().empty()) {
          hierachy.push( { name, Value::Object });
          vis->beginObject(path, parentType == Value::Array, last.isEmpty() || last == parent);
        }
      } else {
        bool first = last.isEmpty() || last == parent;
        if (!hierachy.empty()) {
          vis->record(path, value, hierachy.top().second == Value::Array, first);
        } else {
          vis->record(path, value, false, first);
        }
      }
      last = path;
      ++cnt;
    }

    while (!hierachy.empty()) {
      if (hierachy.top().second == Value::Array) {
        vis->endArray("");
      } else if (hierachy.top().second == Value::Object) {
        vis->endObject("");
      }
      hierachy.pop();
    }

    return 1;
  }


  size_t Janosh::recurseDirectory(Record& travRoot, PrintVisitor* vis, Value::Type rootType, ostream& out) {
    JANOSH_TRACE( { travRoot });

    size_t cnt = 0;
    std::stack<std::pair<const Component, const Value::Type> > hierachy;
    Record rec(travRoot);

    Path last;
    do {
      rec.fetch();
      const Path& path = rec.path();
      const Value& value = rec.value();
      const Value::Type& t = rec.getType();
      const Path& parent = path.parent();

      const Component& name = path.name();
      const Component& parentName = parent.name();

      if (!hierachy.empty()) {
        if (!travRoot.isAncestorOf(path)) {
          break;
        }

        if (!last.above(path) && ((!last.isDirectory() && parentName != last.parentName()) || (last.isDirectory() && parentName != last.name()))) {
          while (!hierachy.empty() && hierachy.top().first != parentName) {
            if (hierachy.top().second == Value::Array) {
              vis->endArray(path);
            } else if (hierachy.top().second == Value::Object) {
              vis->endObject(path);
            }
            hierachy.pop();
          }
        }
      }

      if (t == Value::Array) {
        Value::Type parentType;
        if (hierachy.empty())
          parentType = rootType;
        else
          parentType = hierachy.top().second;

        hierachy.push( { name, Value::Array });
        vis->beginArray(path, parentType == Value::Array, last.isEmpty() || last == parent);
      } else if (t == Value::Object) {
        Value::Type parentType;
        if (hierachy.empty())
          parentType = rootType;
        else
          parentType = hierachy.top().second;

        hierachy.push( { name, Value::Object });
        vis->beginObject(path, parentType == Value::Array, last.isEmpty() || last == parent);
      } else {
        bool first = last.isEmpty() || last == parent;
        if (!hierachy.empty()) {
          vis->record(path, value, hierachy.top().second == Value::Array, first);
        } else {
          vis->record(path, value, false, first);
        }
      }
      last = path;
      ++cnt;
    } while (rec.step());

    while (!hierachy.empty()) {
      if (hierachy.top().second == Value::Array) {
        vis->endArray("");
      } else if (hierachy.top().second == Value::Object) {
        vis->endObject("");
      }
      hierachy.pop();
    }

    return cnt;
  }
  /**
   * Creates a temporary record with the given type.
   * @param t the type of the record to create.
   * @return the created record
   */
  Record Janosh::makeTemp(const Value::Type& t) {
    JANOSH_TRACE({},t);
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
    announceOperation(target.path().pretty(), "A" + lexical_cast<string>(size), Tracker::WRITE);
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

    announceOperation(target.path().pretty(), "O" + lexical_cast<string>(size), Tracker::WRITE);
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

    announceOperation(dest.path().pretty(), value, Tracker::WRITE);
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

    announceOperation(dest.path().pretty(),value, Tracker::WRITE);
    return Record::db.replace(dest.path(), value);
  }


  /**
   * Relaces a destination record with a source record to the new path.
   * If the destination record exists replaces it.
   * @param src source record. Points to the destination record after moving.
   * @param dest destination record.
   * @return 1 if successful, 0 if not
   */
  size_t Janosh::replace(Record& from, Record& dest) {
    JANOSH_TRACE({from, dest});
    Record src = from;
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
        remove(dest,false);
        r = this->copy(src,target);
      } else {
        target = dest.path().basePath();
        remove(dest, false);
        r = this->copy(src,target);
      }

      dest = Record(target.path());
      dest.fetch();
    } else {
      if(src.isDirectory()) {
        Record target = dest.path().asDirectory();
        Record wildcard = src.path().asWildcard();
        remove(dest, false);
        makeDirectory(target, src.getType());
        r = this->append(wildcard, target);
        dest = target;

      } else {
        announceOperation(dest.path().pretty(), src.value(), Tracker::WRITE);
        r = Record::db.replace(dest.path(), src.value());
      }
    }

    from.read();
    dest.read();

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
        announceOperation(dest.path().pretty(), src.value(), Tracker::WRITE);
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

    if(target.isDirectory() || rec.isRange())
      rec.step();

    for(size_t i = 0; i < n; ++i) {
       if(!rec.isDirectory()) {
         if(pack)
           announceOperation(rec.path().pretty(), "", Tracker::DELETE);
         rec.remove();
         ++cnt;
       } else {
         remove(rec,false);
       }
     }

    if(target.isDirectory()) {
      if(pack)
        announceOperation(target.path().pretty(), "", Tracker::DELETE);
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
          Record indexPos;

          if(child.isDirectory()) {
            indexPos = parent.path().withChild(i).asDirectory();
            copy(child, indexPos);
            remove(child, false);
          } else {
            indexPos = parent.path().withChild(i);
            copy(child, indexPos);
            announceOperation(child.path().pretty(),"", Tracker::DELETE);
            child.remove();
          }
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
  size_t Janosh::dump(std::ostream& out) {
    kc::DB::Cursor* cur = Record::db.cursor();
    string key,value;
    cur->jump();
    size_t cnt = 0;

    while(cur->get(&key, &value, true)) {
      out << "path:" << Path(key).pretty() <<  " value:" << value << endl;
      ++cnt;
    }
    delete cur;
    return cnt;
  }

  /**
   * Calculates a hash value over all records
   * @return number of total records hashed.
   */
  size_t Janosh::hash(std::ostream& out) {
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
    out << h << std::endl;
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
      announceOperation(dest.path().withChild(s + cnt).pretty(), *begin, Tracker::WRITE);
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
      else if(src.isRange())
        src.step();

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
          announceOperation(target.pretty(), src.value(), Tracker::WRITE);

          if(!Record::db.add(
              target,
              src.value()
          )) {
            throw janosh_exception() << record_info({"add failed", target});
          }
        } else if(dest.isObject()) {
          Path target = dest.path().withChild(src.path().name());
          announceOperation(target.pretty(), src.value(), Tracker::WRITE);
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


    //Clang workaround for abs ambiguity
    long signed int sz = (long signed int)destIndex - (long signed int)srcIndex;
    if(sz < 0)
      sz *= -1;

    for(int i=0; i < sz; ++i) {
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
    Record tmpDir("/tmp/.");
    remove(tmpDir);
    src = dest;

    return 1;
  }

  void Janosh::setContainerSize(Record container, const size_t s) {
    JANOSH_TRACE({container}, s);
    std::cerr << "size: " << s << std::endl;
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
    announceOperation(container.path().pretty(), lexical_cast<string>(new_value), Tracker::WRITE);
  }

  void Janosh::changeContainerSize(Record container, const size_t by) {
    container.fetch();
    setContainerSize(container, container.getSize() + by);
  }

  size_t Janosh::load(const Path& path, const string& value) {
    announceOperation(path.pretty(), value, Tracker::WRITE);
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

    for(js::Pair& p : obj) {
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

    for(js::Value& v : array){
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

  Janosh* Janosh::getInstance() {
      if(instance_ == NULL)
        instance_ = new Janosh();

      return instance_;
  }

  Janosh* Janosh::instance_ = NULL;
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

void printCommands() {
    std::cerr
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
        <<  "  publish" << endl
        << endl;
      exit(0);
}

using namespace std;
using namespace boost;
using namespace janosh;
namespace po = boost::program_options;

int main(int argc, char** argv) {
  try {
    string command;
    string luafile;
    vector<string> arguments;
    int trackingLevel = 0;

    po::options_description genericDesc("Options");
    genericDesc.add_options()
      ("help,h", "Produce help message")
      ("daemon,d", "Run in daemon mode")
      ("luafile,f", po::value<string>(&luafile), "Run the lua script file")
      ("json,j", "Produce json output")
      ("raw,r", "Produce raw output")
      ("bash,b", "Produce bash output")
      ("triggers,t", "Execute triggers")
      ("verbose,v", "Enable verbose output")
      ("tracing,p", "Enable tracing output")
      ("dblog,m", "Enable db logging")
      ("tracking,k", po::value<int>(&trackingLevel)->composing(), "Print tracking statistics. 0 = Don't print. 1 = Print meta data. 2 = Print full.");

    po::options_description hidden("Hidden options");
    hidden.add_options()
      ("command", po::value< string >(&command), "The command to execute")
      ("arguments", po::value<vector<string> >(&arguments), "Arguments passed to the command");

    po::positional_options_description p;
    p.add("command", 1);
    p.add("arguments", -1);

    po::options_description cmdline_options;
    cmdline_options.add(genericDesc).add(hidden);

    po::options_description visible;
    visible.add(genericDesc);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);
    po::notify(vm);

    janosh::Format f = janosh::Bash;
    bool execTriggers = vm.count("triggers");
    bool verbose = vm.count("verbose");
    bool daemon = vm.count("daemon");
    bool tracing = vm.count("tracing");
    bool dblog = vm.count("dblog");
    if (verbose)
      Logger::init(LogLevel::L_GLOBAL);
    else
      Logger::init(LogLevel::L_ERROR);

    Logger::registerThread("Main");
    Tracker::PrintDirective printDirective = Tracker::DONTPRINT;

    if(trackingLevel == 1)
      printDirective = Tracker::PRINTMETA;
    else if(trackingLevel == 2)
      printDirective = Tracker::PRINTFULL;
    else
      printDirective = Tracker::DONTPRINT;

    if((vm.count("json") && (vm.count("bash") || vm.count("raw"))) || (vm.count("bash") && vm.count("raw"))) {
      LOG_FATAL_STR("Only one format at a time may be specified");
    }

    if(vm.count("json"))
      f = janosh::Json;
    else if(vm.count("bash"))
      f = janosh::Bash;
    else if(vm.count("raw"))
      f = janosh::Raw;

    if(vm.count("daemon") && (vm.count("bash") || vm.count("raw") || vm.count("json") || execTriggers)) {
      LOG_FATAL_STR("Incompatible option(s) conflicting with daemon mode detected");
    }

    if (vm.count("help")) {
        std::cerr << "Usage: janosh [options] command ...\n";
        std::cerr << visible;
        printCommands();
        return 0;
    }

    if (daemon) {
      //initialize the message queue early
      MessageQueue::getInstance();
      Logger::setTracing(tracing);
      Logger::setDBLogging(dblog);
      Tracker::setPrintDirective(printDirective);
      Janosh* instance = Janosh::getInstance();
      instance->open(false);
      if(luafile.empty()) {
        TcpServer* server = TcpServer::getInstance();
        server->open(instance->settings_.port);
        while (server->run()) {
        }
      } else {
        assert(false);
        //FIXME
      }
    } else {
      if (command.empty() && luafile.empty()) {
        throw janosh_exception() << msg_info("missing command");
      }

      if(luafile.empty()) {
        Settings s;
        Request req(f, command, arguments, execTriggers, verbose, get_parent_info());
        TcpClient client;
        client.connect("localhost", s.port);

        return client.run(req, std::cout);
      } else {
        Settings s;
        TcpClient client;

        lua::LuaScript::init([&](){
          client.connect("localhost", s.port);
        },[&](Request& req){
          std::stringstream ss;
          client.run(req, ss);
          return std::make_pair("0", ss.str());
        },[&](){
          client.close();
        });

        lua::LuaScript* script = lua::LuaScript::getInstance();
        script->load(luafile);
        script->run();
      }
    }
  } catch (janosh_exception& ex) {
    printException(ex);
    return 1;
  } catch (std::exception& ex) {
    printException(ex);
    return 1;
  }

  return 0;
}
