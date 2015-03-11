#include "lua_script.hpp"
#include <vector>
#include <sstream>
#include <boost/lexical_cast.hpp>

extern char _binary_JSON_lua_start;
extern char _binary_JSON_lua_end;

extern char _binary_JanoshAPI_lua_start;
extern char _binary_JanoshAPI_lua_end;

namespace janosh {
namespace lua {

using std::vector;

LuaScript* LuaScript::instance = NULL;

static inline std::string &ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
}

static janosh::Request make_request(string command, lua_State* L) {
  std::vector< string > args;
  vector<string> trigger;

  const int len = lua_objlen( L, -1 );
  for ( int i = 1; i <= len; ++i ) {
      lua_pushinteger( L, i );
      lua_gettable( L, -2 );
      args.push_back( lua_tostring( L, -1 ) );
      lua_pop( L, 1 );
  }

  return Request(janosh::Format::Json, command, args, trigger, false, false, get_parent_info());
}

static int l_get(lua_State* L) {
  lua_pushstring(L, (LuaScript::getInstance()->performRequest(make_request("get", L))).c_str());
  return 1;
}

static int l_set(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("set", L));
  return 0;
}

static int l_trigger(lua_State* L) {
  std::vector< string > args;
  vector<string> trigger;

  const int len = lua_objlen( L, -1 );
  for ( int i = 1; i <= len; ++i ) {
      lua_pushinteger( L, i );
      lua_gettable( L, -2 );
      args.push_back( lua_tostring( L, -1 ) );
      lua_pop( L, 1 );
  }

  Request req(janosh::Format::Json, "set", args, trigger, true, false, get_parent_info());
  LuaScript::getInstance()->performRequest(req);
  return 0;
}

static int l_load(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("load", L));
  return 0;
}

static int l_remove(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("remove", L));
  return 0;
}

static int l_append(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("append", L));
  return 0;
}

static int l_add(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("add", L));
  return 0;
}

static int l_replace(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("replace", L));
  return 0;
}

static int l_dump(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("dump", L));
  return 0;
}

static int l_shift(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("shift", L));
  return 0;
}

static int l_mkarr(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("mkarr", L));
  return 0;
}

static int l_mkobj(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("mkobj", L));
  return 0;
}

static int l_size(lua_State* L) {
  std::string strSize = LuaScript::getInstance()->performRequest(make_request("size", L));
  size_t size = boost::lexical_cast<size_t>(trim(strSize));
  lua_pushinteger(L, size);
  return 1;
}

static int l_copy(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("copy", L));
  return 0;
}

static int l_move(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("move", L));
  return 0;
}

static int l_truncate(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("truncate", L));
  return 0;
}

static int l_hash(lua_State* L) {
  lua_pushstring(L, (LuaScript::getInstance()->performRequest(make_request("hash", L))).c_str());
  return 1;
}

LuaScript::LuaScript(std::function<string(janosh::Request&)> requestCallback) : requestCallback(requestCallback) {
  L = luaL_newstate();
  luaL_openlibs(L);
  lua_pushcfunction(L, l_load);
  lua_setglobal(L, "janosh_load");
  lua_pushcfunction(L, l_set);
  lua_setglobal(L, "janosh_set");
  lua_pushcfunction(L, l_add);
  lua_setglobal(L, "janosh_add");
  lua_pushcfunction(L, l_trigger);
  lua_setglobal(L, "janosh_trigger");
  lua_pushcfunction(L, l_replace);
  lua_setglobal(L, "janosh_replace");
  lua_pushcfunction(L, l_append);
  lua_setglobal(L, "janosh_append");
  lua_pushcfunction(L, l_dump);
  lua_setglobal(L, "janosh_dump");
  lua_pushcfunction(L, l_size);
  lua_setglobal(L, "janosh_size");
  lua_pushcfunction(L, l_get);
  lua_setglobal(L, "janosh_get");
  lua_pushcfunction(L, l_copy);
  lua_setglobal(L, "janosh_copy");
  lua_pushcfunction(L, l_remove);
  lua_setglobal(L, "janosh_remove");
  lua_pushcfunction(L, l_shift);
  lua_setglobal(L, "janosh_shift");
  lua_pushcfunction(L, l_move);
  lua_setglobal(L, "janosh_move");
  lua_pushcfunction(L, l_truncate);
  lua_setglobal(L, "janosh_truncate");
  lua_pushcfunction(L, l_mkarr);
  lua_setglobal(L, "janosh_mkarr");
  lua_pushcfunction(L, l_mkobj);
  lua_setglobal(L, "janosh_mkobj");
  lua_pushcfunction(L, l_hash);
  lua_setglobal(L, "janosh_hash");

  std::stringstream ss;
  char*  p = &_binary_JSON_lua_start;
  while ( p != &_binary_JSON_lua_end ) ss << *p++;
  luaL_loadstring(L, ss.str().c_str());
  if(lua_pcall(L, 0, 1, 0)) {
    LOG_ERR_MSG("Preloading JSON library failed", lua_tostring(L, -1));
  }
  lua_setglobal(L, "JSON");

  ss.str("");

  p = &_binary_JanoshAPI_lua_start;
  while ( p != &_binary_JanoshAPI_lua_end ) ss << *p++;
  luaL_loadstring(L, ss.str().c_str());
  if(lua_pcall(L, 0, 1, 0)) {
    LOG_ERR_MSG("Preloading JanoshAPI library failed", lua_tostring(L, -1));
  }
  lua_setglobal(L, "Janosh");
}

LuaScript::~LuaScript() {
	if(L) lua_close(L);
}

void LuaScript::load(const std::string& path) {
  if (luaL_loadfile(L, path.c_str())) {
    LOG_ERR_MSG("Failed to load script", lua_tostring(L, -1));
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
