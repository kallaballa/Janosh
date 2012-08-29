#include "json_spirit.h"
#include <stack>
#include <fstream>
#include <iostream>
#include <sstream>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <kcpolydb.h>

using std::cerr;
using std::cout;
using std::endl;
using std::string;

namespace janosh {
  namespace kc = kyotocabinet;
  namespace js = json_spirit;

  class DBPath: public std::stack<string> {
  public:
    DBPath(const string& strPath) :
        std::stack<string>() {
      using namespace boost;

      char_separator<char> ssep("/");
      tokenizer<char_separator<char> > slashes(strPath, ssep);

      BOOST_FOREACH (const string& s, slashes) {
        char_separator<char> asep("[");
        tokenizer<char_separator<char> > arrays(s, asep);

        BOOST_FOREACH (const string& a, arrays) {
          this->c.push_back((boost::format("/%s") % a).str());
        }
      }
    }

    DBPath() :
        std::stack<string>() {
    }

    string str() const {
      std::stringstream ss;

      for (std::deque<string>::const_iterator it = this->c.begin();
          it != this->c.end(); ++it) {
        ss << *it;
      }
      return ss.str();
    }

    void pushMember(const string& name) {
      std::stack<string>::push((boost::format("/%s") % name).str());
    }

    void pushIndex(const size_t& index) {
      std::stack<string>::push((boost::format("[%d]") % index).str());
    }
  };

  class JsonPrintVisitor {
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

    std::ostream& out;
    std::stack<Frame> hierachy;
    bool currentIsEmpty;

  public:
    JsonPrintVisitor(std::ostream& out) :
        out(out),
        currentIsEmpty(true) {
    }

    void record(const string& key, const string& value) {
      DBPath p(key);
      string name = p.top();

      p.pop();
      string parent;

      if (!p.empty())
        parent = p.top();

      if (!hierachy.empty()) {
        string upper;
        if (name == "/.") {
          p.pop();
          if (!p.empty()) {
            upper = p.top();
          }
        } else {
          upper = parent;
        }

        if (hierachy.top().name != upper) {
          if (hierachy.top().type == Array) {
            this->out << " ] " << endl;
          } else if (hierachy.top().type == Object) {
            this->out << " } " << endl;
          }
          hierachy.pop();
        }
      }

      if (name == "/.") {
        if (!currentIsEmpty) {
          this->out << "," << endl;
        }
        if (value.at(0) == 'A') {
          hierachy.push(Frame(parent, Array));

          if (parent.length() == 0)
            this->out << "[ " << endl;
          else
            this->out << '"' << parent.erase(0, 1) << "\": [ " << endl;
        } else if (value.at(0) == 'O') {
          hierachy.push(Frame(parent, Object));

          if (parent.length() == 0)
            this->out << "{ " << endl;
          else
            this->out << '"' << parent.erase(0, 1) << "\": { " << endl;
        }
        currentIsEmpty = true;

      } else {
        if (!hierachy.empty()) {
          if (hierachy.top().type == Array) {
            if (!currentIsEmpty) {
              this->out << ',' << endl;
            }

            this->out << '\"' << value << '\"';
          } else if (hierachy.top().type == Object) {
            if (!currentIsEmpty) {
              this->out << ',' << endl;
            }

            this->out << '"' << name.erase(0, 1) << "\":\"" << value << "\"";
          }
        }

        currentIsEmpty = false;
      }
    }

    void close() {
      while (!hierachy.empty()) {
        if (hierachy.top().type == Array) {
          this->out << " ] ";
          if(hierachy.size() > 0)
            this->out << "," << endl;
          else
            this->out << endl;
        } else if (hierachy.top().type == Object) {
          this->out << " } ";
          if(hierachy.size() > 0)
            this->out << "," << endl;
          else
            this->out << endl;
        }
        hierachy.pop();
      }
    }

    bool level() {
      return hierachy.empty();
    }
  };

  template<typename printable>
  void error(const string& message, printable p) {
    cerr << message << ": " << p << endl;
    exit(3);
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
      kc::DB::Cursor* cur = db.cursor();
      cur->jump();
      string ckey, cvalue;
      while (cur->get(&ckey, &cvalue, true)) {
        cout << ckey << " = " << cvalue << endl;

      }
      cout << "######## " << endl;
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

      while (cur->get(&key, &value, true)) {
        vis.record(key, value);
        if(vis.level())
          break;
      }

      if (delete_cursor)
        delete cur;

      vis.close();
    }

    void get(const DBPath& path, string& value) {
      string key = path.str();
      string cvalue;
      string top = path.top();
      if (top == "/.") {
        kc::DB::Cursor* cur = db.cursor();
        cur->jump(key.c_str());
        cur->get(&key, &cvalue);

        std::stringstream ss;
        traverseDeep(JsonPrintVisitor(ss), cur);
        value = ss.str();
        delete cur;
      } else {
        db.get(path.str().c_str(), &value);
      }
    }

    void set(const string& path, const string& value) {
      db.set(path, value);
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
        this->set(path.str(), v.get_str());
      }
    }

    void load(js::Object& obj, DBPath& path) {
      path.pushMember(".");
      this->set(path.str(), (boost::format("O%d") % obj.size()).str());
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
      this->set(path.str(), (boost::format("A%d") % array.size()).str());
      path.pop();

      BOOST_FOREACH(js::Value& v, array){
        path.pushIndex(index++);
        load(v, path);
        path.pop();
      }
    }
  };
}
