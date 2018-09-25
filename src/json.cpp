#include <iostream>
#include "exception.hpp"
#include "json.hpp"
#include "value.hpp"

namespace janosh {

JsonPrintVisitor::JsonPrintVisitor(std::ostream& out) : PrintVisitor(out) {
}

string JsonPrintVisitor::escape(const std::string &s) const {
    std::string o;
    o.reserve(s.size() * 2);
    char hexbuffer [4];
    for (auto c = s.cbegin(); c != s.cend(); ++c)  {
        switch (*c) {
        case '"': o.append("\\\""); break;
        case '\\': o.append("\\\\"); break;
        case '\b': o.append("\\b"); break;
        case '\f': o.append("\\f"); break;
        case '\n': o.append("\\n"); break;
        case '\r': o.append("\\r"); break;
        case '\t': o.append("\\t"); break;
        default:
            if ('\x00' <= *c && *c <= '\x1f') {
              sprintf (hexbuffer, "%04x", (int)*c);
              o.append("\\u");
              o += hexbuffer;
            } else {
                o.push_back(*c);
            }
        }
    }
    return o;
}

void JsonPrintVisitor::beginArray(const Path& p, bool parentIsArray, bool first) {
  string name = p.name().pretty();
  if (!first) {
    this->out << ',' << '\n';
  }

  if (parentIsArray)
    this->out << "[ " << '\n';
  else
    this->out << '"' << escape(name) << "\": [ " << '\n';
}

void JsonPrintVisitor::endArray(const Path& p) {
  this->out << " ] " << '\n';
}

void JsonPrintVisitor::beginObject(const Path& p, bool parentIsArray, bool first) {
  string name = p.name().pretty();
  if (!first) {
    this->out << ',' << '\n';
  }

  if (parentIsArray)
    this->out << "{ " << '\n';
  else
    this->out << '"' << escape(name) << "\": { " << '\n';
}

void JsonPrintVisitor::endObject(const Path& p) {
  this->out << " } " << '\n';
}

void JsonPrintVisitor::record(const Path& p, const Value& value, bool parentIsArray, bool first) {
  string name = escape(p.name().pretty());
  string jsonValue = escape(value.str());

  if (parentIsArray) {
    if (!first) {
      this->out << ',' << '\n';
    }
    if(value.getType() == Value::Number || value.getType() == Value::Boolean)
      this->out << jsonValue;
    else
      this->out << '\"' << jsonValue << '\"';

  } else {
    if (!first) {
      this->out << ',' << '\n';
    }
    if(value.getType() == Value::Number || value.getType() == Value::Boolean)
      this->out << '"' << name << "\":" << jsonValue;
    else
      this->out << '"' << name << "\":\"" << jsonValue << "\"";
  }
}
}
