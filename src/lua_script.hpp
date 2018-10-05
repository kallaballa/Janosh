#ifndef LUASCRIPT_H
#define LUASCRIPT_H

#include <string>
#include <vector>
#include <iostream>
#include "logger.hpp"
#include "request.hpp"
#include <mutex>
#include <condition_variable>
#include <lua.hpp>
#include <thread>
#include <deque>

namespace janosh {
namespace lua {

class LuaScript {
public:
  LuaScript(std::function<void()> openCallback,
        std::function<std::pair<int,string>(janosh::Request&)> requestCallback,
        std::function<void(bool)> closeCallback, lua_State* l = NULL);
    ~LuaScript();

    void defineMacros(const std::vector<std::pair<string,string>>& macros);
    void makeGlobalVariable(const string& key, const string& value);
    void load(const string& path);
    void loadString(const string& luaCode);
    void run();
    void clean();
    void performOpen(const string& strID, bool lockRequest = true);
    void performClose(bool lockRequest, bool commit);
    void printTransactions(std::ostream& os);
    std::pair<int, string>  performRequest(janosh::Request req, bool doTransaction);

    static void init(std::function<void()> openCallback,
        std::function<std::pair<int,string>(janosh::Request&)> requestCallback,
        std::function<void(bool)> closeCallback) {
      assert(instance_ == NULL);
      instance_ = new LuaScript(openCallback,requestCallback,closeCallback);
    }


    static LuaScript* getInstance() {
      assert(instance_ != NULL);
      return instance_;
    }
    std::function<void()> openCallback_;
    std::function<std::pair<int,string>(janosh::Request&)> requestCallback_;
    std::function<void(bool)> closeCallback_;
    lua_State* L;
private:
    static LuaScript* instance_;
    std::mutex open_lock_;
    std::condition_variable open_lock_cond_;
    std::deque<std::pair<std::thread::id, string>> open_queue_;
    std::thread::id open_current_id;
    bool isOpen = false;

};
}
}
#endif
