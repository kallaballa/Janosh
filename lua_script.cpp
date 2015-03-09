#include "lua_script.hpp"
#include <vector>
#include <sstream>

namespace janosh {
namespace lua {

using std::vector;

LuaScript* LuaScript::instance = NULL;

static int l_get(lua_State* L) {
  std::vector< string > args;
  vector<string> trigger;

  const int len = lua_objlen( L, -1 );
  for ( int i = 1; i <= len; ++i ) {
      lua_pushinteger( L, i );
      lua_gettable( L, -2 );
      args.push_back( lua_tostring( L, -1 ) );
      lua_pop( L, 1 );
  }

  string command = "get";
  janosh::Request req(janosh::Format::Json, command, args, trigger, false, false, get_parent_info());
  string result = LuaScript::getInstance()->performRequest(req);
  lua_pushstring(L, result.c_str());
  return 1;
}

static int l_set(lua_State* L) {
  std::vector< string > args;
  vector<string> trigger;

  const int len = lua_objlen( L, -1 );
  for ( int i = 1; i <= len; ++i ) {
      lua_pushinteger( L, i );
      lua_gettable( L, -2 );
      args.push_back( lua_tostring( L, -1 ) );
      lua_pop( L, 1 );
  }

  string command = "set";
  janosh::Request req(janosh::Format::Json, command, args, trigger, true, false, get_parent_info());
  string result = LuaScript::getInstance()->performRequest(req);
  return 0;
}

LuaScript::LuaScript(std::function<string(janosh::Request&)> requestCallback) : requestCallback(requestCallback) {
  L = luaL_newstate();
  lua_pushcfunction(L, l_get);
  lua_setglobal(L, "get");
  lua_pushcfunction(L, l_set);
  lua_setglobal(L, "set");
}

LuaScript::~LuaScript() {
	if(L) lua_close(L);
}

void LuaScript::load(const std::string& path) {
  if (luaL_loadfile(L, path.c_str())) {
    LOG_ERR_MSG("Failed to load script", lua_tostring(L, -1));
  } else {
    if(L) luaL_openlibs(L);
  }
}

void LuaScript::run() {
  if(lua_pcall(L, 0, 0, 0)) {
    LOG_ERR_MSG("Lua script failed", lua_tostring(L, -1));
  }
}

string LuaScript::performRequest(janosh::Request req) {
  return requestCallback(req);
}

std::vector<std::string> LuaScript::getTableKeys(const std::string& name) {
    std::string code = 
        "function getKeys(name) "
        "s = \"\""
        "for k, v in pairs(_G[name]) do "
        "    s = s..k..\",\" "
        "    end "
        "return s "
        "end"; // function for getting table keys
    luaL_loadstring(L, 
        code.c_str()); // execute code
    lua_pcall(L,0,0,0);
    lua_getglobal(L, "getKeys"); // get function
    lua_pushstring(L, name.c_str());
    lua_pcall(L, 1 , 1, 0); // execute function
    std::string test = lua_tostring(L, -1);
    std::vector<std::string> strings;
    std::string temp = "";
    std::cout<<"TEMP:"<<test<<std::endl;
    for(unsigned int i = 0; i < test.size(); i++) {     
        if(test.at(i) != ',') {
            temp += test.at(i);
        } else {
            strings.push_back(temp);
            temp= "";
        }
    }
    clean();
    return strings;
}
}
}
