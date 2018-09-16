#include <iostream>
#include <ucontext.h>
#include <sstream>
#include "exception.hpp"

#ifdef _JANOSH_BACKTRACE
#define BACKWARD_HAS_BFD 1
#define BACKWARD_HAS_DW 1
#include "backward.hpp"
#endif

namespace janosh {
janosh_exception::janosh_exception() {
#ifdef _JANOSH_BACKTRACE
  using namespace backward;
  StackTrace st; st.load_here(32);
  Printer p;
  p.object = true;
  p.color_mode = ColorMode::always;
  p.address = true;
  p.print(st, stderr);
#endif
}

void printException(std::exception& ex) {
  LOG_DEBUG_MSG("Exception", ex.what());
}

void printException(janosh::janosh_exception& ex) {
  std::stringstream ss;
  printException(ex, ss);
  LOG_DEBUG_STR(ss.str());
}

void printException(janosh::janosh_exception& ex, std::ostream& os) {
  using namespace boost;

  os << "Exception: " << std::endl;

  if (auto const* m = get_error_info<msg_info>(ex))
    os << "message: " << *m << std::endl;

  if (auto const* vs = get_error_info<string_info>(ex)) {
    for (auto it = vs->begin(); it != vs->end(); ++it) {
      os << "info: " << *it << std::endl;
    }
  }

  if (auto const* ri = get_error_info<record_info>(ex))
    os << ri->first << ": " << ri->second << std::endl;

  if (auto const* pi = get_error_info<path_info>(ex))
    os << pi->first << ": " << pi->second << std::endl;

  if (auto const* vi = get_error_info<value_info>(ex))
    os << vi->first << ": " << vi->second << std::endl;
}
}

