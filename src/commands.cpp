#include "commands.hpp"
#include "exception.hpp"
#include "request.hpp"
#include <sys/stat.h>
#include <sstream>

namespace janosh {

class FilterCommand: public Command {
public:
  explicit FilterCommand(Janosh* janosh) :
      Command(janosh) {
  }

  Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.size() < 1) {
      return {-1, "Expected a list of keys and a filter expression"};
    } else {
      std::vector<Record> recs;
      for (size_t i = 0; i < params.size() -1; ++i)  {
        const Value& p = params[i];
        recs.push_back(Record(p.str()));
      }

      if (!janosh->filter(recs, params.back().str(), out))
        return {-1, "Fetching failed"};
    }
    return {params.size(), "Successful"};
  }
};
class RandomCommand: public Command {
public:
  explicit RandomCommand(Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const std::vector<Value>& params, std::ostream& out) {
    if (params.size() == 1) {
      Record rec(params[0].str());
      if(rec.isDirectory()) {
        return {janosh->random(rec, out), "Successful"};
      }
      else
        return {-1, "Expected a directory"};
    } else if(params.size() == 2) {
      Record rec(params[0].str());
      if(rec.isDirectory()) {
        return {janosh->random(rec, params[1].str(), out), "Successful"};
      }
      else
        return {-1, "Expected a directory"};
    } else {
      return {-1, "Only one or two parameters expected"};
    }
  }
};

class ExistsCommand: public Command {
public:
  explicit ExistsCommand(Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const std::vector<Value>& params, std::ostream& out) {
    if (params.size() == 1) {
      Record rec(params[0].str());
      rec.fetch();
      if(rec.exists())
        return {1, "Successful"};
      else
        return {-1, "Not found"};
    } else {
      return {-1, "Only one parameter expected"};
    }
  }
};

class RemoveCommand: public Command {
public:
  explicit RemoveCommand(Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const std::vector<Value>& params, std::ostream& out) {
    if (!params.empty()) {
      int cnt = 0;
      for(const Value& p : params) {
        LOG_DEBUG_MSG("Removing", p.str());
//FIXME use cursors for batch operations
        Record path(p.str());
        cnt += janosh->remove(path);
        LOG_DEBUG(cnt);
      }

      if (cnt == 0)
        return {0, "No entries removed"};
      else
        return {cnt, "Successful"};
    } else {
      return {-1, "Too few parameters"};
    }
  }
};
class HashCommand: public Command {
public:
  explicit HashCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (!params.empty()) {
      LOG_DEBUG_STR("hash doesn't take any parameters");
      return {-1, "Failed"};
    } else {
      return {janosh->hash(out), "Successful"};
    }
  }
};

inline bool file_exists (const std::string& name) {
  struct stat buffer;
  return (stat (name.c_str(), &buffer) == 0);
}

class PatchCommand: public Command {
public:
  explicit PatchCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.empty()) {
      janosh->patch(std::cin);
    } else {
      for(const Value& p : params) {
        if(file_exists(p.str())) {
          janosh->patch(p.str());
        } else  {
          std::stringstream ss(p.str());
          janosh->patch(ss);
        }
      }
    }
    return {0, "Successful"};
  }
};

class LoadCommand: public Command {
public:
  explicit LoadCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.empty()) {
      janosh->patch(std::cin);
    } else {
      for(const Value& p : params) {
        if(file_exists(p.str())) {
          janosh->loadJson(p.str());
        } else  {
          std::stringstream ss(p.str());
          janosh->loadJson(ss);
        }
      }
    }
    return {0, "Successful"};
  }
};

class MakeArrayCommand: public Command {
public:
  explicit MakeArrayCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.size() != 1)
      return {-1, "Expected one path"};

    if (janosh->makeArray(Record(params.front().str())))
      return {1, "Successful"};
    else
      return {-1, "Failed"};
  }
};

class MakeObjectCommand: public Command {
public:
  explicit MakeObjectCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.size() != 1)
      return {-1, "Expected one path"};

    if (janosh->makeObject(Record(params.front().str())))
      return {1, "Successful"};
    else
      return {-1, "Failed"};
  }
};

class PublishCommand: public Command {
public:
  explicit PublishCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    size_t s = params.size();
    if (s > 3 || s < 1)
      return {-1, "Expected one, two or three parameters (key, operation, value)"};

    if(s == 3)
      janosh->publish(params[0].str(), params[1].str(), params[2].str());
    else if(s == 2)
      janosh->publish(params[0].str(), "W", params[1].str());
    else if(s == 1)
      janosh->publish(params[0].str(), "W", "");
    else
      assert(false);
    return {1, "Successful"};
  }
};


class TruncateCommand: public Command {
public:
  explicit TruncateCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (!params.empty())
      return {-1, "Truncate doesn't take any arguments"};

    return {janosh->truncate(), "Successful"};
  }
};

class AddCommand: public Command {
public:
  explicit AddCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.size() % 2 != 0) {
      return {-1, "Expected a list of path/value pairs"};
    } else {
      const string path = params.front().str();

      for (auto it = params.begin(); it != params.end(); it += 2) {
        if (!janosh->add(Record((*it).str()), (*(it + 1))))
          return {-1, "Failed"};
      }

      return {params.size()/2, "Successful"};
    }
  }
};

class ReplaceCommand: public Command {
public:
  explicit ReplaceCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.empty() || params.size() % 2 != 0) {
      return {-1, "Expected a list of path/value pairs"};
    } else {

      for (auto it = params.begin(); it != params.end(); it += 2) {
        if (!janosh->replace(Record((*it).str()), (*(it + 1))))
          return {-1, "Failed"};
      }

      return {params.size()/2, "Successful"};
    }
  }
};

class SetCommand: public Command {
public:
  explicit SetCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.empty() || params.size() % 2 != 0) {
      return {-1, "Expected a list of path/value pairs"};
    } else {

      size_t cnt = 0;
      for (auto it = params.begin(); it != params.end(); it += 2) {
        cnt += janosh->set(Record((*it).str()), (*(it + 1)));
      }

      if (cnt == 0)
        return {-1, "Failed"};
      else
        return {cnt, "Successful"};
    }
  }
};

class ShiftCommand: public Command {
public:
  explicit ShiftCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.size() != 2) {
      return {-1, "Expected two paths"};
    } else {
      Record src(params.front().str());
      Record dest(params.back().str());
      src.fetch();

      if (!src.exists())
        return {-1, "Source path doesn't exist"};

      size_t cnt = janosh->shift(src, dest);
      if (cnt == 0)
        return {-1, "Failed"};
      else
        return {1, "Successful"};
    }
  }
};

class CopyCommand: public Command {
public:
  explicit CopyCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.size() != 2) {
      return {-1, "Expected two paths"};
    } else {
      Record src(params.front().str());
      Record dest(params.back().str());
      src.fetch();

      if (!src.exists())
        return {-1, "Source path doesn't exist"};

      size_t n = janosh->copy(src, dest);
      if (n > 0)
        return {n, "Successful"};
      else
        return {-1, "Failed"};
    }
  }
};

class MoveCommand: public Command {
public:
  explicit MoveCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.size() != 2) {
      return {-1, "Expected two paths"};
    } else {
      Record src(params.front().str());
      Record dest(params.back().str());
      src.fetch();

      if (!src.exists())
        return {-1, "Source path doesn't exist"};

      size_t n = janosh->move(src, dest);
      if (n > 0)
        return {n, "Successful"};
      else
        return {-1, "Failed"};
    }
  }
};

class AppendCommand: public Command {
public:
  explicit AppendCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.size() < 2) {
      return {-1, "Expected a path and a list of values"};
    } else {
      Record target(params.front().str());
      size_t cnt = janosh->append(params.begin() + 1, params.end(), target);
      return {cnt, "Successful"};
    }
  }
};

class DumpCommand: public Command {
public:
  explicit DumpCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (!params.empty()) {
      return {-1, "Dump doesn't take any parameters"};
    } else {
      janosh->dump(out);
    }
    return {0, "Successful"};
  }
};

class SizeCommand: public Command {
public:
  explicit SizeCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  virtual Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.size() != 1) {
      return {-1, "Expected a path"};
    } else {
      Record p(params.front().str());
      out << janosh->size(p) << '\n';
    }
    return {0, "Successful"};
  }
};

class GetCommand: public Command {
public:
  explicit GetCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  Result operator()(const vector<Value>& params, std::ostream& out) {
    if (params.empty()) {
      return {-1, "Expected a list of keys"};
    } else {
      std::vector<Record> recs;
      for(const Value& p : params) {
        recs.push_back(Record(p.str()));
      }

      if (!janosh->get(recs, out))
        return {-1, "Fetching failed"};
    }
    return {params.size(), "Successful"};
  }
};

class ReportCommand: public Command {
public:
  explicit ReportCommand(janosh::Janosh* janosh) :
      Command(janosh) {
  }

  Result operator()(const vector<Value>& params, std::ostream& out) {
    if (!params.empty()) {
      return {-1, "status command doesn't take any arguments"};
    } else {
      janosh->report(out);
    }
    return {1, "Successful"};
  }
};


CommandMap makeCommandMap(Janosh* janosh) {
  CommandMap cm;
  cm.insert( { "load", new LoadCommand(janosh) });
  cm.insert( { "set", new SetCommand(janosh) });
  cm.insert( { "add", new AddCommand(janosh) });
  cm.insert( { "replace", new ReplaceCommand(janosh) });
  cm.insert( { "append", new AppendCommand(janosh) });
  cm.insert( { "dump", new DumpCommand(janosh) });
  cm.insert( { "size", new SizeCommand(janosh) });
  cm.insert( { "get", new GetCommand(janosh) });
  cm.insert( { "copy", new CopyCommand(janosh) });
  cm.insert( { "remove", new RemoveCommand(janosh) });
  cm.insert( { "shift", new ShiftCommand(janosh) });
  cm.insert( { "move", new MoveCommand(janosh) });
  cm.insert( { "truncate", new TruncateCommand(janosh) });
  cm.insert( { "mkarr", new MakeArrayCommand(janosh) });
  cm.insert( { "mkobj", new MakeObjectCommand(janosh) });
  cm.insert( { "hash", new HashCommand(janosh) });
  cm.insert( { "publish", new PublishCommand(janosh) });
  cm.insert( { "exists", new ExistsCommand(janosh) });
  cm.insert( { "random", new RandomCommand(janosh) });
  cm.insert( { "filter", new FilterCommand(janosh) });
  cm.insert( { "patch", new PatchCommand(janosh) });
  cm.insert( { "report", new ReportCommand(janosh) });


  return cm;
}
}
