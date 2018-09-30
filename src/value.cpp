#include "value.hpp"
#include <boost/lexical_cast.hpp>
#include "exception.hpp"

using namespace janosh;

Value::Value() :
    strObj(), type(Null), size(0), initalized(false) {
}

Value::Value(Type t) :
    type(t), initalized(true){
  init("", t == String || t == Number || t == Boolean);
}

Value::Value(const string& v, Type t) :
    type(t), initalized(true) {
  init(v, t == String || t == Number || t == Boolean);
}

Value::Value(const string v, bool dir) :
    initalized(true) {
  init(v, !dir);
}

Value::Value(const Value& other) {
  this->strObj = other.strObj;
  this->type = other.type;
  this->size = other.size;
  this->initalized = other.initalized;
}

bool Value::isInitialized() const {
  return initalized;
}

bool Value::isEmpty() const {
  return strObj.empty();
}

void Value::reset() {
  this->strObj.clear();
  this->initalized = false;
}

string Value::str() const {
  if(!isInitialized())
    throw value_exception() << value_info({"uinitialized value", "null"});
//    if(type == Boolean || type == String || type == Number)
//      return this->strObj;
//    else
      return this->strObj;
}

const Value::Type Value::getType() const {
  return this->type;
}

const size_t Value::getSize() const {
  if(!isInitialized())
    throw value_exception() << value_info({"uinitialized value", "null"});

  if(isEmpty())
    throw value_exception() << value_info({"empty value", "null"});

  return this->size;
}

void Value::init(string v, bool value) {
  if (!value) {
    assert(!v.empty());
    char c = v.at(0);
    if (c == 'A') {
      this->type = Array;
    } else if (c == 'O') {
      this->type = Object;
    } else {
      throw value_exception() << value_info({"unknown directory descriptor", boost::lexical_cast<string>(c)});
    }

    this->size = boost::lexical_cast<size_t>(v.substr(1));
  } else {
    this->size = 1;;
  }

  this->strObj = v;
}

string Value::makeDBString() const{
    if(type == String)
      return "s" + str();
    else if(type == Number)
      return "n" + str();
    else if(type == Boolean)
      return "b" + str();

    return str();
}

