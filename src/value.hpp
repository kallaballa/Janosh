#ifndef VALUE_HPP_
#define VALUE_HPP_

#include <string>
#include "path.hpp"

namespace janosh {
  using std::string;

  class Value {
  public:
    enum Type {
      Null, String, Array, Object, Range, Number, Boolean
    };

    Value();
    explicit Value(Type t);
    Value(const string& v, Type t);
    Value(const string v, bool dir);
    Value(const Value& other);
    bool isInitialized() const;
    bool isEmpty() const;
    void reset();
    string str() const;
    const Type getType()  const;
    const size_t getSize() const;
    void init(string v, bool value);
    string makeDBString() const;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
         ar & strObj;
         ar & type;
         ar & size;
         ar & initalized;
     }
  private:
    string strObj;
    Type type;
    size_t size;
    bool initalized;


  };
}
#endif /* VALUE_HPP_ */
