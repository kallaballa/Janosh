#ifndef EXITHANDLER_HPP_
#define EXITHANDLER_HPP_

#include <stddef.h>
#include <vector>
#include <functional>

namespace janosh {
using std::function;
using std::vector;

class ExitHandler {
public:
  typedef std::function<void()> ExitFunc;
  static ExitHandler* getInstance();
  void addExitFunc(ExitFunc e);
private:
  static ExitHandler* instance_;
  ExitHandler();
};

} /* namespace janosh */

#endif /* EXITHANDLER_HPP_ */
