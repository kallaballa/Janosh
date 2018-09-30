#include <iostream>
#include "exception.hpp"
#include "json.hpp"
#include "value.hpp"

namespace janosh {

JsonPrintVisitor::JsonPrintVisitor(std::ostream& out) : PrintVisitor(out) {
}



void JsonPrintVisitor::beginArray(const Path& p, bool parentIsArray, bool first) {
  string name = escape_json(p.name().pretty());
  if (!first) {
    this->out << ',' << '\n';
  }

  if (parentIsArray)
    this->out << "[ " << '\n';
  else
    this->out << '"' << name << "\": [ " << '\n';
}

void JsonPrintVisitor::endArray(const Path& p) {
  this->out << " ] " << '\n';
}

void JsonPrintVisitor::beginObject(const Path& p, bool parentIsArray, bool first) {
  string name = escape_json(p.name().pretty());
  if (!first) {
    this->out << ',' << '\n';
  }

  if (parentIsArray)
    this->out << "{ " << '\n';
  else
    this->out << '"' << name << "\": { " << '\n';
}

void JsonPrintVisitor::endObject(const Path& p) {
  this->out << " } " << '\n';
}

void JsonPrintVisitor::record(const Path& p, const Value& value, bool parentIsArray, bool first) {
  string name = escape_json(p.name().pretty());
  string jsonValue = escape_json(value.str());

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
