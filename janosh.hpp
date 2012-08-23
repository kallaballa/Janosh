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

using namespace json_spirit;
using std::cerr;
using std::cout;
using std::endl;
using std::string;

using namespace kyotocabinet;

class Path : public std::stack<string> {
public:
  Path(const string& strPath) : std::stack<string>() {
    using namespace boost;

    char_separator<char> ssep("/");
    tokenizer<char_separator<char> > slashes(strPath, ssep);

    BOOST_FOREACH (const string& s, slashes){
      char_separator<char> asep("[");
      tokenizer<char_separator<char> > arrays(s, asep);

      BOOST_FOREACH (const string& a, arrays){
        this->c.push_back((boost::format("/%s") % a).str());
      }
    }
  }

  Path() : std::stack<string>() {
  }

  string str() const {
    std::stringstream ss;

    for(std::deque<string>::const_iterator it = this->c.begin(); it != this->c.end(); ++it) {
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

template<class map_type = std::map<string, Value *> >
class Janosh {
  typedef map_type Index;

public:
  Janosh(const string& filename) :
      filename(filename) {
  }

  void load() {
    // open the database
    if (!db.open("db.chk", PolyDB::OWRITER | PolyDB::OCREATE)) {
      cerr << "open error: " << db.error().name() << endl;
    }

    std::ifstream is(filename.c_str());
    read(is, rootValue);
    Path path;
    load(rootValue, path);
  }

  void save() {
    std::ofstream os(filename.c_str());
    write(rootValue, os);
  }

  void dump() {
    DB::Cursor* cur = db.cursor();
    cur->jump();
    string ckey, cvalue;
    while (cur->get(&ckey, &cvalue, true)) {
      cout << ckey << " = " << cvalue << endl;

    }
    cout << "######## " << endl;
    delete cur;
  }

  void get(const Path& path, string& value) {
    string key = path.str();
    string cvalue;
    string top = path.top();
    if(top == "/.") {
      std::stringstream ss;

      DB::Cursor* cur = db.cursor();
      cur->jump(key.c_str());
      cur->get(&key, &cvalue);

      size_t len = boost::lexical_cast<size_t>(cvalue);
      for (size_t i = 0; i < len; ++i) {
        cur->step();
        cur->get_key(&key);
        cur->get_value(&cvalue);
        ss  << key << " = " << cvalue << endl;
        skipSubElements(cur);
      }

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
  TreeDB db;
  string filename;
  Value rootValue;

  bool skipSubElements(DB::Cursor* cur) {
    string key;
    cur->get_key(&key);

    if (boost::algorithm::ends_with(key, "/.")) {
      string subval;
      cur->get_value(&subval);
      size_t sublen = boost::lexical_cast<size_t>(subval);
      for (size_t j = 0; j < sublen; ++j) {
        cur->step();
        skipSubElements(cur);
      }
      return true;
    }
    else {
      return false;
    }
  }

  void load(Value& v, Path& path) {
    if (v.type() == obj_type) {
      load(v.get_obj(), path);
    } else if (v.type() == array_type) {
      load(v.get_array(), path);
    } else {
      this->set(path.str(), v.get_str());
    }
  }

  void load(Object& obj, Path& path) {
    path.pushMember(".");
    this->set(path.str(), (boost::format("%d") % obj.size()).str());
    path.pop();

    BOOST_FOREACH(Pair& p, obj) {
        path.pushMember(p.name_);
        load(p.value_, path);
        path.pop();
    }
  }

  void load(Array& array, Path& path) {
    int index = 0;
    path.pushMember(".");
    this->set(path.str(), (boost::format("%d") % array.size()).str());
    path.pop();

    BOOST_FOREACH(Value& v, array) {
      path.pushIndex(index++);
      load(v, path);
      path.pop();
    }
  }
};
