#ifndef _JANOSH_JSON_HPP
#define _JANOSH_JSON_HPP

#include "record.hpp"
#include <string>
#include <stack>
#include <iostream>

using std::string;
using std::endl;

namespace janosh {
  class JsonPrintVisitor {
    std::ostream& out;

    const string escape(const string& s) {
      size_t index = 0;
      string ns = s;
      while (true) {
           index = ns.find("\"", index);
           if (index == string::npos) break;
           ns.replace(index, 1, "\\\"");
           index+=2;
      }
      return ns;
    }
  public:
    JsonPrintVisitor(std::ostream& out) :
        out(out){
    }

    void beginArray(const Path& p, bool parentIsArray, bool first) {
      string name = p.name().pretty();
      if (!first) {
        this->out << ',' << endl;
      }

      if (parentIsArray)
        this->out << "[ " << endl;
      else
        this->out << '"' << name << "\": [ " << endl;
    }

    void endArray(const Path& p) {
      this->out << " ] " << endl;
    }

    void beginObject(const Path& p, bool parentIsArray, bool first) {
      string name = p.name().pretty();
      if (!first) {
        this->out << ',' << endl;
      }

      if (parentIsArray)
        this->out << "{ " << endl;
      else
        this->out << '"' << name << "\": { " << endl;
    }

    void endObject(const Path& p) {
      this->out << " } " << endl;
    }

    void record(const Path& p, const string& value, bool parentIsArray, bool first) {
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

    void begin() {
    }

    void close() {
    }
  };
}

#endif
