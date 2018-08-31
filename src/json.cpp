#include <iostream>
#include "json.hpp"

namespace janosh {

JsonPrintVisitor::JsonPrintVisitor(std::ostream& out) : PrintVisitor(out) {
}

string JsonPrintVisitor::escape(const std::string &s) const {
    std::ostringstream o;
    for (auto c = s.cbegin(); c != s.cend(); c++) {
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
void JsonPrintVisitor::beginArray(const Path& p, bool parentIsArray, bool first) {
  string name = p.name().pretty();
  if (!first) {
    this->out << ',' << endl;
  }

  if (parentIsArray)
    this->out << "[ " << endl;
  else
    this->out << '"' << name << "\": [ " << endl;
}

void JsonPrintVisitor::endArray(const Path& p) {
  this->out << " ] " << endl;
}

void JsonPrintVisitor::beginObject(const Path& p, bool parentIsArray, bool first) {
  string name = p.name().pretty();
  if (!first) {
    this->out << ',' << endl;
  }

  if (parentIsArray)
    this->out << "{ " << endl;
  else
    this->out << '"' << name << "\": { " << endl;
}

void JsonPrintVisitor::endObject(const Path& p) {
  this->out << " } " << endl;
}

void JsonPrintVisitor::record(const Path& p, const string& value, bool parentIsArray, bool first) {
  string name = p.name().pretty();
  string jsonValue = escape(value);

  if (parentIsArray) {
    if (!first) {
      this->out << ',' << endl;
    }

    this->out << '\"' << jsonValue << '\"';
  } else {
    if (!first) {
      this->out << ',' << endl;
    }

    this->out << '"' << name << "\":\"" << jsonValue << "\"";
  }
}
}
