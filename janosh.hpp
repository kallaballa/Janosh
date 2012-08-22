#include "json_spirit.h"
#include <deque>
#include <stack>
#include <fstream>
#include <iostream>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

using namespace json_spirit;
using std::cerr;
using std::cout;
using std::endl;
using std::string;

class Janosh {

public:
  Janosh(const string& filename) : filename(filename) {
  }

  void load() {
    std::ifstream is(filename.c_str());
    read(is, rootValue);
  }

  void save() {
    std::ofstream os(filename.c_str());
    write(rootValue, os);
  }

  bool get(const string& strPath, string& strValue) {
    Path p = parsePath(strPath);
    return get(rootValue.get_array(), p, strValue);
  }

  bool set(const string& strPath, const string& strValue) {
    Path p = parsePath(strPath);
    return set(p, strValue, rootValue.get_array());
  }

private:
  string filename;
  Value rootValue;
  typedef std::stack<string, std::deque<string> > Path;

  Path parsePath(const string& strPath) {
      using namespace boost;

      char_separator<char> sep("/");
      tokenizer<char_separator<char> > tokens(strPath, sep);
      std::deque<string> components;

      BOOST_FOREACH (const string& t, tokens) {
        components.push_front(t);
      }
      reverse(components.begin(), components.end());

      return Path(components);
    }

  bool set(Path p, const string& strValue, Object& obj) {
    string component = p.top();
    p.pop();

    for (Object::size_type i = 0; i != obj.size(); ++i) {
      Pair& pair = obj[i];
      string& name = pair.name_;
      Value& value = pair.value_;
      if (name == component) {
        if(p.empty()) {
          obj[i].value_ = Value(strValue);
          return true;
        } else if ((value.type() == obj_type && set(p, strValue, value.get_obj()))
            || (value.type() == array_type && set(p, strValue,value.get_array()))) {
          return true;
        }
        return false;
      }
    }

    return false;
  }

  bool set(Path p, const string& strValue, Array& array) {
	  for(Array::iterator it = array.begin(); it != array.end(); it++) {
		  if (!p.empty()) {
			if(set(p, strValue, (*it).get_obj()))
				return true;
		  }
		  else {
			*it = Value(strValue);
			return true;
		  }
		}

    return false;
  }

  bool get(const Object& obj, Path p, string& strValue) {
    string component = p.top();
    p.pop();

    for (Object::size_type i = 0; i != obj.size(); ++i) {
      const Pair& pair = obj[i];
      const string& name = pair.name_;
      const Value& value = pair.value_;
      if (name == component) {
        if(p.empty()) {
          strValue = value.get_str();
          return true;
        } else if ((value.type() == obj_type && get(value.get_obj(), p, strValue))
            || (value.type() == array_type && get(value.get_array(), p, strValue))) {
          return true;
        }
        return false;
      }
    }

    return false;
  }

  bool get(const Array& array, Path p, string& strValue) {
    BOOST_FOREACH(const Value& v, array)
    {
      if (!p.empty()) {
        if(get(v.get_obj(), p, strValue))
        	return true;
      }
      else {
        strValue = v.get_str();
        return true;
      }
    }

    return false;
  }
};
