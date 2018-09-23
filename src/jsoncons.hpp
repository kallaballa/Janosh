#ifndef _JANOSH_JSONCONS_HPP
#define _JANOSH_JSONCONS_HPP

#include "print_visitor.hpp"
#include "value.hpp"
#include <jsoncons/json.hpp>
#include <stack>


namespace janosh {
using std::string;
using namespace jsoncons;

class JsonPathVisitor : public PrintVisitor {
    json root_;
    std::stack<json*> hierachy_;
    string escape(const string& s) const;
  public:
    explicit JsonPathVisitor(std::ostream& out);
    virtual void beginArray(const Path& p, bool parentIsArray, bool first) override;
    virtual void endArray(const Path& p)  override;
    virtual void beginObject(const Path& p, bool parentIsArray, bool first) override;
    virtual void endObject(const Path& p) override;
    virtual void record(const Path& p, const Value& value, bool parentIsArray, bool first) override;
    json* getRoot() {
      return &root_;
    }
  };
}

#endif
