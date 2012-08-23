#include "json_spirit.h"
#include <stack>
#include <map>
#include <fstream>
#include <iostream>
#include <sstream>
#include <boost/foreach.hpp>
#include <boost/unordered_map.hpp>
#include <boost/tokenizer.hpp>
#include <boost/function.hpp>

using namespace json_spirit;
using std::cerr;
using std::cout;
using std::endl;
using std::string;
using boost::function;

class Path : public std::stack<string> {
  string strPath;
public:
  Path(const string& strPath) : std::stack<string>(), strPath(strPath) {
    using namespace boost;

    char_separator<char> sep("/");
    tokenizer<char_separator<char> > tokens(strPath, sep);

    BOOST_FOREACH (const string& t, tokens){
      this->c.push_front(t);
    }
  }

  Path() : std::stack<string>() {
  }

  string str() const {
    std::stringstream ss;
    BOOST_FOREACH (const string& t, this->c){
      ss << t;
    }
    return ss.str();
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
    std::ifstream is(filename.c_str());
    read(is, rootValue);
    Path path;
    buildIndex(rootValue, path);
  }

  void save() {
    std::ofstream os(filename.c_str());
    write(rootValue, os);
  }

  const string& get(const Path& path) {
    return idx[path.str()]->get_str();
  }

  void set(Path path, const string& strValue) {
    idx[path.str()]->operator =(Value(strValue));
  }

private:
  Index idx;
  string filename;
  Value rootValue;

  void buildIndex(Value& v, Path& path) {
    idx[path.str()] = &v;

    if (v.type() == obj_type) {
      buildIndex(v.get_obj(), path);
    } else if (v.type() == array_type) {
      buildIndex(v.get_array(), path);
    }
  }

  void buildIndex(Object& obj, Path& path) {
    BOOST_FOREACH(Pair& p, obj) {
        path.push(p.name_);
        buildIndex(p.value_, path);
        path.pop();
    }
  }

  void buildIndex(Array& array, Path& path) {
    BOOST_FOREACH(Value& v, array) {
      buildIndex(v, path);
    }
  }
};

