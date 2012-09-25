#ifndef _JANOSH_HPP
#define _JANOSH_HPP

#include "json_spirit.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <exception>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <kcpolydb.h>

#include "dbpath.hpp"
#include "json.hpp"
#include "bash.hpp"

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

  enum Format {
    Bash,
    Json,
    Raw
  };

  class TriggerBase {
    typedef std::map<string,string> TargetMap;
    typedef typename TargetMap::value_type Target;

    vector<fs::path> targetDirs;
    map<DBPath, std::set<string> > triggers;
    TargetMap targets;
public:
    TriggerBase(const fs::path& config, const vector<fs::path>& targetDirs);

    int executeTarget(const string& name);

    void executeTrigger(const DBPath p);
    bool findAbsoluteCommand(const string& cmd, string& abs);
    void load(const fs::path& config);
    void load(std::ifstream& is);
    template<typename T> void error(const string& msg, T t, int exitcode=1) {
      LOG_ERR_MSG(msg, t);
      exit(exitcode);
    }
  };

  class Settings {
  public:
    fs::path janoshFile;
    fs::path databaseFile;
    fs::path triggerFile;
    vector<fs::path> triggerDirs;

    Settings();
    template<typename T> void error(const string& msg, T t, int exitcode=1) {
      LOG_ERR_MSG(msg, t);
      exit(exitcode);
    }
  private:
    bool find(const js::Object& obj, const string& name, js::Value& value);
  };

  class RawPrintVisitor {
    std::ostream& out;

  public:
    RawPrintVisitor(std::ostream& out) :
        out(out){
    }

    void beginArray(const string& key, bool first) {
    }

    void endArray(const string& key) {
    }

    void beginObject(const string& key, bool first) {
    }

    void endObject(const string& key) {
    }

    void begin() {
    }

    void close() {
    }

    void record(const string& key, const string& value, bool array, bool first) {
        string stripped = value;
        replace(stripped.begin(), stripped.end(), '\n', ' ');
        out << stripped << endl;
    }
  };

  class Janosh {
  public:
    Settings settings;
    TriggerBase triggers;
    Format format;
    Janosh();

    void setFormat(Format f) ;
    Format getFormat();

    template<typename T> void error(const string& msg, T t, int exitcode=1) {
      LOG_ERR_MSG(msg, t);
      close();
      exit(exitcode);
    }

    void open();
    void loadJson(const string& jsonfile);
    void loadJson(std::istream& is);
    bool print(const DBPath& path, std::ostream& out, Format f=Bash);
    void set(DBPath dbPath, const string& value, bool check=true);
    void close();
    size_t remove(const DBPath& path, kc::DB::Cursor* cur = NULL);
    void move(const DBPath& from, const DBPath& to);
    void dump();
  private:
    kc::TreeDB db;
    string filename;
    js::Value rootValue;

    bool get(const string& key, string* value, bool check = true);
    void setContainerSize(const DBPath& container, const size_t& s, kc::DB::Cursor* cur = NULL);
    void changeContainerSize(const DBPath& container, const size_t& by, kc::DB::Cursor* cur = NULL);
    void load(js::Value& v, DBPath& path);
    void load(js::Object& obj, DBPath& path);
    void load(js::Array& array, DBPath& path);

    template<typename Tvisitor>
     void recurse(Tvisitor vis, kc::DB::Cursor* cur = NULL) {
       std::stack<std::pair<const string&, const EntryType&> > hierachy;

       string key;
       string value;
       bool delete_cursor = false;

       if (!cur) {
         cur = db.cursor();
         cur->jump();
         delete_cursor = true;
       }

       vis.begin();
       cur->get(&key, &value, true);
       const DBPath travRoot(key);
       DBPath last;

       do {
         if(key == "/.") {
           continue;
         }
         const DBPath p(key);
         const EntryType& t = p.getType(value);
         const string& name = p.name();

         if (!hierachy.empty()) {
           if (!travRoot.above(p)) {
             break;
           }

           if(!last.above(p) && p.parentName() != last.parentName()) {
             while(!hierachy.empty() && hierachy.top().first != p.parentName()) {
               if (hierachy.top().second == EntryType::Array) {
                 vis.endArray(key);
               } else if (hierachy.top().second == EntryType::Object) {
                 vis.endObject(key);
               }
               hierachy.pop();
             }
           }
         }

         if (t == EntryType::Array) {
           hierachy.push({name, EntryType::Array});
           vis.beginArray(key, last.str().empty() || last == p.parent());
         } else if (t == EntryType::Object) {
           hierachy.push({name, EntryType::Object});
           vis.beginObject(key, last.str().empty() || last == p.parent());
         } else {
           bool first = last.str().empty() || last == p.parent();
           if(!hierachy.empty()){
             vis.record(key, value, hierachy.top().second == EntryType::Array, first);
           } else {
             vis.record(key, value, false, first);
           }
         }
         last = p;
       } while (cur->get(&key, &value, true));

       while (!hierachy.empty()) {
           if (hierachy.top().second == EntryType::Array) {
             vis.endArray("");
           } else if (hierachy.top().second == EntryType::Object) {
             vis.endObject("");
           }
           hierachy.pop();
       }

       if (delete_cursor)
         delete cur;

       vis.close();
     }
  };
}
#endif
