#ifndef LUASCRIPT_H
#define LUASCRIPT_H

#include <string>
#include <vector>
#include <iostream>
#include "logger.hpp"
#include "request.hpp"
#include <mutex>
#include <condition_variable>

extern "C" {
# include "luajit-2.0/lua.h"
# include "luajit-2.0/lauxlib.h"
# include "luajit-2.0/lualib.h"
}

namespace janosh {
namespace lua {
class LuaScript {
public:
  LuaScript(std::function<void()> openCallback,
        std::function<std::pair<string,string>(janosh::Request&)> requestCallback,
        std::function<void()> closeCallback);
    ~LuaScript();

    void load(const string& path);
    void loadString(const string& luaCode);
    void run();
    void clean();
    void performOpen(bool lockRequest = true);
    void performClose(bool lockRequest = true);
    string performRequest(janosh::Request req);
    void setLuaChangeCallback(string functionName) {
      luaChangeCallbackName_ = functionName;
    }

    static void init(std::function<void()> openCallback,
        std::function<std::pair<string,string>(janosh::Request&)> requestCallback,
        std::function<void()> closeCallback) {
      instance_ = new LuaScript(openCallback,requestCallback,closeCallback);
    }

    static LuaScript* getInstance() {
      assert(instance_ != NULL);
      return instance_;
    }
    std::function<void()> openCallback_;
    std::function<std::pair<string,string>(janosh::Request&)> requestCallback_;
    std::function<void()> closeCallback_;
    lua_State* L;
private:
    int level_ = 0;
    static LuaScript* instance_;
    std::mutex open_lock_;
    std::condition_variable open_lock_cond_;
    std::queue<std::thread::id> open_queue_;
    volatile bool isOpen = false;

    string lastRevision_;
    string luaChangeCallbackName_;
    std::mutex requestMutex_;
};
}
}
#endif
