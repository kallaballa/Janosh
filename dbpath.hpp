#ifndef _JANOSH_DBPATH_HPP
#define _JANOSH_DBPATH_HPP

#include <stack>
#include <sstream>
#include <iostream>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

using std::string;

namespace janosh {
  class DBPath {
  public:
    std::vector<string> components;
    bool container;

    DBPath(const string& strPath) {
      using namespace boost;

      char_separator<char> ssep("/");
      tokenizer<char_separator<char> > slashes(strPath, ssep);

      BOOST_FOREACH (const string& s, slashes) {
        char_separator<char> asep("[");
        tokenizer<char_separator<char> > arrays(s, asep);

        BOOST_FOREACH (const string& a, arrays) {
          this->components.push_back((boost::format("/%s") % a).str());
        }
      }

      this->container = false;

      if(this->components.size() > 0 && this->name() == "/.")
        this->container = true;
    }

    DBPath(const DBPath& other) {
      this->components = other.components;
    }

    DBPath() {
    }

    bool isContainer() const {
      return container;
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

    const string name() const {
      size_t d = 1;

      if(isContainer())
        ++d;

      if(components.size() >= d)
        return *(components.end() - d);
      else
        return "";
    }

    DBPath parent() const {
      DBPath parent = *this;
      parent.pop();
      if(isContainer() && parent.components.size() > 0)
        parent.pop();

      parent.pushMember(".");

      return parent;
    }

    const string parentName() const {
      size_t d = 2;
      if(isContainer())
        ++d;

      if(components.size() >= d)
        return *(components.end() - d);
      else
        return "";
    }

    const string& root() const {
      return components.front();
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
