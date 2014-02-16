#include <iostream>
#include "exception.hpp"

#ifdef _JANOSH_DEBUG
#include "backward.h"
#endif

namespace janosh {
janosh_exception::janosh_exception() {
#ifdef _JANOSH_DEBUG
  using namespace backward;
  StackTrace st;
  st.load_here(32);

  Printer printer;
  printer.print(st, stderr);
#endif
}

void printException(std::exception& ex) {
  std::cerr << "Exception: " << ex.what() << std::endl;
}

void printException(janosh::janosh_exception& ex) {
  using namespace boost;

  std::cerr << "Exception: " << std::endl;

  if (auto const* m = get_error_info<msg_info>(ex))
    std::cerr << "message: " << *m << std::endl;

  if (auto const* vs = get_error_info<string_info>(ex)) {
    for (auto it = vs->begin(); it != vs->end(); ++it) {
      std::cerr << "info: " << *it << std::endl;
    }
  }

  if (auto const* ri = get_error_info<record_info>(ex))
    std::cerr << ri->first << ": " << ri->second << std::endl;

  if (auto const* pi = get_error_info<path_info>(ex))
    std::cerr << pi->first << ": " << pi->second << std::endl;

  if (auto const* vi = get_error_info<value_info>(ex))
    std::cerr << vi->first << ": " << vi->second << std::endl;
}
}

