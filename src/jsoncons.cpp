#include <iostream>
#include "jsoncons_ext/jsonpath/json_query.hpp"
#include "exception.hpp"
#include "jsoncons.hpp"
#include "value.hpp"

namespace janosh {

using namespace jsoncons;
JsonPathVisitor::JsonPathVisitor(std::ostream& out) : PrintVisitor(out) {
  hierachy_.push(&root_);
}

string JsonPathVisitor::escape(const std::string &s) const {
    std::ostringstream o;
    for (auto c = s.cbegin(); c != s.cend(); c++)  {
        switch (*c) {
        case '"': o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\b': o << "\\b"; break;
        case '\f': o << "\\f"; break;
        case '\n': o << "\\n"; break;
        case '\r': o << "\\r"; break;
        case '\t': o << "\\t"; break;
        default:
            if ('\x00' <= *c && *c <= '\x1f') {
                o << "\\u"
                  << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
            } else {
                o << *c;
            }
        }
    }
    return o.str();
}

void JsonPathVisitor::beginArray(const Path& p, bool parentIsArray, bool first) {
  string name = escape(p.name().pretty());
  json::array arr;

  if((*hierachy_.top()).is_array()) {
    (*hierachy_.top()).push_back(arr);
    hierachy_.push(&(*((*hierachy_.top()).end_elements()-1)));
  } else {
    (*hierachy_.top())[name] = arr;
    auto& ref = (*hierachy_.top()).at(name);
    hierachy_.push(&ref);
  }
}

void JsonPathVisitor::endArray(const Path& p) {
  hierachy_.pop();
}

void JsonPathVisitor::beginObject(const Path& p, bool parentIsArray, bool first) {
  string name = escape(p.name().pretty());
  json::object obj;

  if((*hierachy_.top()).is_array()){
    (*hierachy_.top()).push_back(obj);
    hierachy_.push(&(*((*hierachy_.top()).end_elements()-1)));
  } else {
    (*hierachy_.top())[name] = obj;
    auto& ref = (*hierachy_.top()).at(name);
    hierachy_.push(&ref);
  }
}

void JsonPathVisitor::endObject(const Path& p) {
   hierachy_.pop();
}

void JsonPathVisitor::record(const Path& p, const Value& value, bool parentIsArray, bool first) {
  string name = escape(p.name().pretty());
  string jsonValue = escape(value.str());
  if(parentIsArray) {
    if(value.getType() == Value::Number)
     (*hierachy_.top()).push_back(std::stol(jsonValue));
   else if(value.getType() == Value::Boolean)
     (*hierachy_.top()).push_back((jsonValue == "true"));
   else if(value.getType() == Value::String)
     (*hierachy_.top()).push_back(jsonValue);
  } else {
       if(value.getType() == Value::Number)
        (*hierachy_.top())[name] = std::stol(jsonValue);
      else if(value.getType() == Value::Boolean)
        (*hierachy_.top())[name] = (bool) (jsonValue == "true");
      else if(value.getType() == Value::String)
        (*hierachy_.top())[name] = jsonValue;
  }
}

}
