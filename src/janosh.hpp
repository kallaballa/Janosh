#ifndef _JANOSH_HPP
#define _JANOSH_HPP

#include <map>
#include <vector>
#include <string>
#include <iostream>

#include "settings.hpp"
#include "record.hpp"
#include "format.hpp"
#include "print_visitor.hpp"
#include "request.hpp"

namespace janosh {

namespace js = json_spirit;
namespace fs = boost::filesystem;
using std::string;
using std::vector;
using std::map;
using std::vector;
using std::istream;
using std::ostream;

class Command;
typedef map<const std::string, Command*> CommandMap;

class Janosh {
public:
  Settings settings_;
  CommandMap cm_;

  Janosh();
  ~Janosh();

  void setFormat(Format f);
  void open(bool readOnly);
  bool isOpen();
  void close();
  bool beginTransaction();
  bool beginTransactionTry();
  void endTransaction(bool commit);
  void publish(const string& key, const string& op, const char* value);
  void publish(const string& key, const string& op, const string& value);

  size_t process(int argc, char** argv);

  size_t patch(const string& jsonfile);
  size_t patch(istream& is);
  size_t loadJson(const string& jsonfile);
  size_t loadJson(istream& is);

  size_t makeArray(Record target, size_t size = 0, bool boundsCheck = true);
  size_t makeObject(Record target, size_t size = 0);
  size_t makeDirectory(Record target, Value::Type type, size_t size = 0);
  size_t filter(vector<Record> targets, const std::string& jsonPathExps, std::ostream& out);
  size_t get(vector<Record> targets, std::ostream& out);
  size_t get(vector<Record> recs, PrintVisitor* vis,std::ostream& out);
  size_t size(Record target);
  size_t remove(Record& target, bool pack = true);
  size_t random(Record rec, std::ostream& out);
  size_t random(Record rec, const string& jsonPathExpr, std::ostream& out);

  size_t add(Record target, const Value& value);
  size_t replace(Record target, const Value& value);
  size_t set(Record target, const Value& value);
  size_t append(Record target, const Value& value);
  size_t append(vector<Value>::const_iterator begin, vector<Value>::const_iterator end, Record dest);

  size_t move(Record& src, Record& dest);
  size_t replace(Record& src, Record& dest);
  size_t append(Record& src, Record& dest);
  size_t copy(Record& src, Record& dest);
  size_t shift(Record& src, Record& dest);

  size_t dump(ostream& out);
  size_t hash(ostream& out);
  void report(ostream& out);
  size_t truncate();

private:
  Format format;
  bool open_;
  string filename;
  js::Value rootValue;

  Format getFormat();
  void setContainerSize(Record rec, const size_t s);
  void changeContainerSize(Record rec, const size_t by);
  size_t patch(const Path& path, const Value& value);
  size_t patch(js::Value& v, Path& path);
  size_t patch(js::Object& obj, Path& path);
  size_t patch(js::Array& array, Path& path);
  size_t load(const Path& path, const Value& value);
  size_t load(js::Value& v, Path& path);
  size_t load(js::Object& obj, Path& path);
  size_t load(js::Array& array, Path& path);

  bool boundsCheck(Record p);
  Record makeTemp(const Value::Type& t);

  size_t recurseDirectory(Record& travRoot, PrintVisitor* vis, Value::Type rootType, ostream& out);
  size_t recurseValue(Record& travRoot, PrintVisitor* vis, Value::Type rootType, ostream& out);

};

}
#endif
