#ifndef LUASCRIPT_H
#define LUASCRIPT_H

#include <string>
#include <vector>
#include <iostream>
#include "logger.hpp"
#include "request.hpp"

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
        std::function<string(janosh::Request&)> requestCallback,
        std::function<void()> closeCallback);
    ~LuaScript();

    void load(const string& path);
    void run();
    void performOpen();
    void performClose();
    string performRequest(janosh::Request req);

    std::vector<std::string> getTableKeys(const std::string& name);
    
    inline void clean() {
      int n = lua_gettop(L);
      lua_pop(L, n);
    }

    template<typename T>
    T get(const std::string& variableName) {
      if(!L) {
        LOG_ERR_MSG("Script is not loaded", variableName);
        return lua_getdefault<T>();
      }
      
      T result;
      if(lua_gettostack(variableName)) { // variable succesfully on top of stack
        result = lua_get<T>(variableName);  
      } else {
        result = lua_getdefault<T>();
      }

     
      clean();
      return result;
    }

    bool lua_gettostack(const std::string& variableName) {
      level_ = 0;
      std::string var = "";
        for(unsigned int i = 0; i < variableName.size(); i++) {
          if(variableName.at(i) == '.') {
            if(level_ == 0) {
              lua_getglobal(L, var.c_str());
            } else {
              lua_getfield(L, -1, var.c_str());
            }
            
            if(lua_isnil(L, -1)) {
              LOG_ERR_MSG(var + " is not defined", variableName);
              return false;
            } else {
              var = "";
              level_++;
            }
          } else {
            var += variableName.at(i);
          }
        }
        if(level_ == 0) {
          lua_getglobal(L, var.c_str());
        } else {
          lua_getfield(L, -1, var.c_str());
        }
        if(lua_isnil(L, -1)) {
          LOG_ERR_MSG(var + " is not defined", variableName);
          return false;
        }

        return true;       
    }

    // Generic get
    template<typename T>
    T lua_get(const std::string& variableName) {
      return 0;
    }

    template<typename T>
    T lua_getdefault() {
      return 0;
    }
   
    static void init(std::function<void()> openCallback,
        std::function<string(janosh::Request&)> requestCallback,
        std::function<void()> closeCallback) {
      instance_ = new LuaScript(openCallback,requestCallback,closeCallback);
    }

    static LuaScript* getInstance() {
      assert(instance_ != NULL);
      return instance_;
    }
    lua_State* L;
private:
    int level_ = 0;
    static LuaScript* instance_;
    std::function<void()> openCallback_;
    std::function<string(janosh::Request&)> requestCallback_;
    std::function<void()> closeCallback_;
    bool isOpen = false;
};

// Specializations
template <> 
inline bool LuaScript::lua_get<bool>(const std::string& variableName) {
    return (bool)lua_toboolean(L, -1);
}

template <> 
inline float LuaScript::lua_get<float>(const std::string& variableName) {
    if(!lua_isnumber(L, -1)) {
      LOG_ERR_MSG("Not a number", variableName);
    }
    return (float)lua_tonumber(L, -1);
}

template <>
inline int LuaScript::lua_get<int>(const std::string& variableName) {
    if(!lua_isnumber(L, -1)) {
      LOG_ERR_MSG("Not a number", variableName);
    }
    return (int)lua_tonumber(L, -1);
}

template <>
inline std::string LuaScript::lua_get<std::string>(const std::string& variableName) {
    std::string s = "null";
    if(lua_isstring(L, -1)) {
      s = std::string(lua_tostring(L, -1));
    } else {
      LOG_ERR_MSG("Not a string", variableName);
    }
    return s;
}

template<>
inline std::string LuaScript::lua_getdefault<std::string>() {
  return "null";
}

}
}
#endif
