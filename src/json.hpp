#ifndef _JANOSH_JSON_HPP
#define _JANOSH_JSON_HPP

#include "print_visitor.hpp"
#include "value.hpp"

namespace janosh {
using std::string;



class JsonPrintVisitor : public PrintVisitor {
    string escape(const string& s) const;
  public:
    explicit JsonPrintVisitor(std::ostream& out);
    virtual void beginArray(const Path& p, bool parentIsArray, bool first) override;
    virtual void endArray(const Path& p)  override;
    virtual void beginObject(const Path& p, bool parentIsArray, bool first) override;
    virtual void endObject(const Path& p) override;
    virtual void record(const Path& p, const Value& value, bool parentIsArray, bool first) override;
  };
}

#endif
