#ifndef _JANOSH_HPP
#define _JANOSH_HPP

#include "json_spirit.h"
#include <fstream>
#include <iostream>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <kcpolydb.h>
#include <boost/filesystem.hpp>

#include "dbpath.hpp"
#include "json.hpp"
#include "bash.hpp"

using std::cerr;
using std::cout;
using std::endl;
using std::string;

namespace janosh {
  namespace js = json_spirit;
  namespace fs = boost::filesystem;

  class TriggerBase {
    typedef std::map<string,string> TargetMap;
    typedef typename TargetMap::value_type Target;

    vector<fs::path> targetDirs;
    map<DBPath, std::set<string> > triggers;
    TargetMap targets;
public:
    TriggerBase(const fs::path& config, const vector<fs::path>& targetDirs) :
      targetDirs(targetDirs) {
      load(config);
    }

    int executeTarget(const string& name) {
      auto it = targets.find(name);
      if(it == targets.end())
        error("Target not found", name);

      janosh::verbose("target", name);
      return system((*it).second.c_str());
    }

    void executeTrigger(const DBPath p) {
      auto it = triggers.find(p);
      if(it != triggers.end()) {
        BOOST_FOREACH(const string& name, (*it).second) {
          janosh::verbose("trigger" + name);
          executeTarget(name);
        }
      }
    }

    bool findAbsoluteCommand(const string& cmd, string& abs) {
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

    void load(const fs::path& config) {
      ifstream is(config.string().c_str());
      load(is);
      is.close();
    }

    void load(std::ifstream& is) {
      try {
        js::Value triggerConf;
        js::read(is, triggerConf);

        BOOST_FOREACH(js::Pair& p, triggerConf.get_obj()) {
          string name = p.name_;
          js::Array arrCmdToTrigger = p.value_.get_array();

          if(arrCmdToTrigger.size() < 2)
            error("Illegal target", name);

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
  };

  enum Format {
    Bash,
    Json,
    Raw
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

  namespace kc = kyotocabinet;
  namespace js = json_spirit;

  bool VERBOSE = false;

  template<typename printable>
  void error(const string& message, printable p) {
    cerr << message << ": " << p << endl;
    exit(3);
  }

  template<typename printable>
  void verbose(const string& message, printable p) {
    if(VERBOSE)
      cerr << message << ": " << p << endl;
  }

  void verbose(const string& message) {
    if(VERBOSE)
      cerr << message << endl;
  }

  class Janosh {
  public:
    Janosh() {
    }

    void open(const string& dbfile) {
      // open the database
      if (!db.open(dbfile, kc::PolyDB::OREADER | kc::PolyDB::OWRITER | kc::PolyDB::OCREATE)) {
        error("open error", db.error().name());
      }
    }

    void loadJson(const string& jsonfile) {
      std::ifstream is(jsonfile.c_str());
      this->loadJson(is);
      is.close();
    }

    void loadJson(std::istream& is) {
      js::Value rootValue;
      js::read(is, rootValue);

      DBPath path;
      load(rootValue, path);
    }

    void dump() {
      kc::DB::Cursor* cur;
      cur = db.cursor();
      cur->jump();
      string key, value;
      while(cur->get(&key, &value, true)) {
        cout << "key: " << key << " value: " << value << endl;
      }
      delete cur;
    }

    template<typename Tvisitor>
    void traverseDeep(Tvisitor vis, kc::DB::Cursor* cur = NULL) {
      string key;
      string value;
      bool delete_cursor = false;

      if (!cur) {
        cur = db.cursor();
        cur->jump();
        delete_cursor = true;
      }

      enum container_type {
        Object, Array, None
      };

      struct Frame {
        string name;
        container_type type;

        Frame(string name, container_type type) :
            name(name), type(type) {
        }
      };

      std::stack<Frame> hierachy;

      vis.begin();
      cur->get(&key, &value, false);
      DBPath travRoot(key);
      DBPath last;

      while (cur->get(&key, &value, true)) {
        if(key == "/.") {
          continue;
        }
        DBPath p(key);
        string name = p.name();
        string parent = p.parentName();

        if (!hierachy.empty()) {
          if (!travRoot.above(p)) {
            //cout << endl;
            break;
          }

          if(!last.above(p) && parent != last.parentName()) {
            while(hierachy.size() > 0 && hierachy.top().name != p.parentName()) {
              if (hierachy.top().type == Array) {
                vis.endArray(key);
              } else if (hierachy.top().type == Object) {
                vis.endObject(key);
              }
              hierachy.pop();
            }
          }
        }

        if (p.isContainer()) {
          if (value.at(0) == 'A') {
            hierachy.push(Frame(name, Array));
            vis.beginArray(key, last.str().empty() || last == p.parent());
          } else if (value.at(0) == 'O') {
            hierachy.push(Frame(name, Object));
            vis.beginObject(key, last.str().empty() || last == p.parent());
          }
        } else {

          bool first = last.str().empty() || last == p.parent();
          vis.record(key, value, hierachy.top().type == Array, first);
        }

        last = p;
      }

      while (!hierachy.empty()) {
          if (hierachy.top().type == Array) {
            vis.endArray("");
          } else if (hierachy.top().type == Object) {
            vis.endObject("");
          }
          hierachy.pop();
      }

      if (delete_cursor)
        delete cur;

      vis.close();
    }

    bool get(const DBPath& path, string& value, Format f=Bash) {
      std::stringstream ss;
      bool found = get(path,ss, f);
      value = ss.str();
      return found;
    }

    bool get(const DBPath& path, std::ostream& out, Format f=Bash) {
      verbose("get", path.str());
      string key = path.str();
      string cvalue;

      if (path.isContainer()) {
        verbose("traverse", path.str());
        kc::DB::Cursor* cur = db.cursor();
        cur->jump(key.c_str());
        bool found = cur->get(&key, &cvalue);

        if(found) {
          switch(f) {
            case Json:
              traverseDeep(JsonPrintVisitor(out), cur);
              break;
            case Bash:
              traverseDeep(BashPrintVisitor(out), cur);
              break;
            case Raw:
              traverseDeep(RawPrintVisitor(out), cur);
              break;
          }

        }

        delete cur;
        return found;
      } else {
        verbose("db.get", path.str());
        string value;
        bool found = db.get(path.str().c_str(), &value);

        if(found) {
          switch(f) {
          case Raw:
          case Json:
            out << value << endl;
            break;
          case Bash:
            out << "\"( [" << key << "]='" << value << "' )\"" << endl;
            break;
          }
        }

        return found;
      }
    }

    void set(const string& path, const string& value, bool check=true) {
      DBPath dbPath(path);
      DBPath dbParent = dbPath.parent();
      string parent = dbParent.str();

      if(check && db.check(path) == -1) {
        if(db.check(parent) == -1) {
          error("Unkown key", path);
        } else {
          string value;
          kc::DB::Cursor* cur = db.cursor();
          cur->jump(parent);
          cur->get(&parent, &value,false);
          char t = value.at(0);
          value.erase(value.begin());
          size_t len = boost::lexical_cast<size_t>(value);
          this->set(parent, (boost::format("%c%d") % t % ++len).str());
          delete cur;
        }
      }

      db.set(path, value);
    }

    void close() {
      db.close();
    }
  private:
    kc::TreeDB db;
    string filename;
    js::Value rootValue;

    bool skipSubElements(kc::DB::Cursor* cur) {
      string key;
      cur->get_key(&key);

      if (boost::algorithm::ends_with(key, "/.")) {
        string subval;
        cur->get_value(&subval);
        size_t sublen = boost::lexical_cast<size_t>(subval.erase(0, 1));
        for (size_t j = 0; j < sublen; ++j) {
          cur->step();
          skipSubElements(cur);
        }
        return true;
      } else {
        return false;
      }
    }

    void load(js::Value& v, DBPath& path) {
      if (v.type() == js::obj_type) {
        load(v.get_obj(), path);
      } else if (v.type() == js::array_type) {
        load(v.get_array(), path);
      } else {
        this->set(path.str(), v.get_str(), false);
      }
    }

    void load(js::Object& obj, DBPath& path) {
      path.pushMember(".");
      this->set(path.str(), (boost::format("O%d") % obj.size()).str(), false);
      path.pop();

      BOOST_FOREACH(js::Pair& p, obj) {
        path.pushMember(p.name_);
        load(p.value_, path);
        path.pop();
      }
    }

    void load(js::Array& array, DBPath& path) {
      int index = 0;
      path.pushMember(".");
      this->set(path.str(), (boost::format("A%d") % array.size()).str(), false);
      path.pop();

      BOOST_FOREACH(js::Value& v, array){
        path.pushIndex(index++);
        load(v, path);
        path.pop();
      }
    }
  };
}
#endif
