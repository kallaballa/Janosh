
#include "util.hpp"
#include "exception.hpp"

// Don't use /proc, lol.
#if defined(__APPLE__) && defined(__MACH__)
#include <crt_externs.h>
#endif


namespace janosh {

string read_proc_val(const string& filename) {
  string v;
  ifstream in(filename, std::ios::in | std::ios::binary);

  try {
    if (in) {
      getline(in, v);
      in.close();
      return v;
    }
  } catch(std::exception& ex) {
  }

  throw janosh_exception() << string_info({"Unable to get value from", filename});
  return v;
}

ProcessInfo get_process_info(pid_t pid) {
  ProcessInfo pinfo;
  pinfo.pid_ = pid;

#if defined(__APPLE__) && defined(__MACH__)
  char **argv = *_NSGetArgv();
  pinfo.cmdline_ = argv[0];
#else
  string procDir = "/proc/" + std::to_string(pid) + "/";
  pinfo.cmdline_ = read_proc_val(procDir + "cmdline");
#endif

  return pinfo;
}

ProcessInfo get_parent_info() {
  return get_process_info(getppid());
}

} /* namespace janosh */
