#ifndef LUASCRIPT_H
#define LUASCRIPT_H

#include <string>
#include <vector>
#include <iostream>
#include "logger.hpp"
#include "request.hpp"
#include <mutex>
#include <condition_variable>
#include  <lua.hpp>
#include <thread>

class _XDisplay;

namespace janosh {
namespace lua {

class LuaScript {
public:
  LuaScript(std::function<void()> openCallback,
        std::function<std::pair<string,string>(janosh::Request&)> requestCallback,
        std::function<void()> closeCallback, lua_State* l = NULL);
    ~LuaScript();

    void load(const string& path);
    void loadString(const string& luaCode);
    void run();
    void clean();
    void performOpen(bool lockRequest = true);
    void performClose(bool lockRequest = true);
    string performRequest(janosh::Request req);

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
#ifndef JANOSH_NO_X11
    _XDisplay* display_;
    unsigned long rootWin_;
#endif
private:
    int level_ = 0;
    static LuaScript* instance_;
    std::mutex open_lock_;
    std::condition_variable open_lock_cond_;
    std::queue<std::thread::id> open_queue_;
    std::thread::id open_current_id;
    bool isOpen = false;

};
}
}
#endif
