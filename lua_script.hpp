#ifndef LUASCRIPT_H
#define LUASCRIPT_H

#include <string>
#include <vector>
#include <iostream>
#include "logger.hpp"
#include "request.hpp"
#include <mutex>

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
    void performOpen();
    void performClose();
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
    bool isOpen = false;
    string lastRevision_;
    string luaChangeCallbackName_;
    std::mutex requestMutex_;
};
}
}
#endif
