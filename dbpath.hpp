#ifndef _JANOSH_DBPATH_HPP
#define _JANOSH_DBPATH_HPP

#include <stack>
#include <sstream>
#include <iostream>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include "Logger.hpp"

using std::string;

namespace janosh {
  enum EntryType {
    Array,
    Object,
    Value
  };

  class DBPath {
  public:
    std::vector<string> components;

    DBPath(const string& strPath) {
      using namespace boost;
      if(strPath.empty() || strPath.at(0) != '/') {
        LOG_ERR_MSG("Illegal Path", strPath);
        exit(1);
      }
      string s  = strPath.substr(1);
      char_separator<char> ssep("/[", "", boost::keep_empty_tokens);
      tokenizer<char_separator<char> > tokComponents(s, ssep);

      BOOST_FOREACH (const string& c, tokComponents) {
        if(c.empty()) {
          LOG_ERR_MSG("Illegal Path", strPath);
          exit(1);
        }
        this->components.push_back((boost::format("/%s") % c).str());
      }
    }

    DBPath(const DBPath& other) {
      this->components = other.components;
    }

    DBPath() {
    }

    bool isContainer() const {
      return !this->components.empty() && this->components.back() == "/.";
    }

    bool operator<(const DBPath& other) const {
      return this->str() < other.str();
    }

    bool operator==(const DBPath& other) const {
      return this->str() == other.str();
    }

    bool operator!=(const DBPath& other) const {
      return this->str() != other.str();
    }

    string str() const {
      std::stringstream ss;

      for (auto it = this->components.begin(); it != this->components.end(); ++it) {
        ss << *it;
      }
      return ss.str();
    }

    void pushMember(const string& name) {
      components.push_back((boost::format("/%s") % name).str());
    }

    void pushIndex(const size_t& index) {
      components.push_back((boost::format("/%d") % index).str());
    }

    void pop() {
      components.erase(components.end() - 1);
    }

    string name() const {
      size_t d = 1;

      if(isContainer())
        ++d;

      if(components.size() >= d)
        return *(components.end() - d);
      else
        return "";
    }

    size_t parseIndex() const {
      return boost::lexical_cast<size_t>(this->name().substr(1));
    }

    DBPath parent() const {
      DBPath parent = *this;
      parent.pop();
      if(isContainer() && parent.components.size() > 0)
        parent.pop();

      parent.pushMember(".");

      return parent;
    }

    string parentName() const {
      size_t d = 2;
      if(isContainer())
        ++d;

      if(components.size() >= d)
        return *(components.end() - d);
      else
        return "";
    }

    string root() const {
      return components.front();
    }

    const EntryType getType(string& value)  const {
      if(this->isContainer()) {
        char c = value.at(0);
        if(c == 'A') {
          return Array;
        } else if(c == 'O') {
          return Object;
        } else {
          assert(!"Unknown container descriptor");
        }
      }

      return Value;
    }

    const size_t getSize(string& value) const {
      if(this->isContainer()) {
        return boost::lexical_cast<size_t>(value.substr(1,value.size() - 1));
      } else {
        return 0;
      }
    }

    bool above(const DBPath& other) const {
      if(other.components.size() >= this->components.size() ) {
        size_t depth = this->components.size();
        if(this->isContainer()) {
          depth--;
        }

        size_t i;
        for(i = 0; i < depth; ++i) {

          const string& tc = this->components[i];
          const string& oc = other.components[i];

          if(oc != tc) {
            return false;
          }
        }

        return true;
      }

      return false;
    }
  };
}

#endif
