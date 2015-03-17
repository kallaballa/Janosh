#include "lua_script.hpp"
#include <vector>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <chrono>
#include <thread>
#include <mutex>
#include <thread>
#include "cppzmq/zmq.hpp"

extern char _binary_JSON_lua_start;
extern char _binary_JSON_lua_end;

extern char _binary_JanoshAPI_lua_start;
extern char _binary_JanoshAPI_lua_end;

namespace janosh {
namespace lua {

void make_subscription(string prefix, string luaCode) {
  std::thread t([=]() {
        zmq::context_t context (1);
        zmq::socket_t subscriber (context, ZMQ_SUB);
        string user = std::getenv("USER");
        subscriber.connect((string("ipc://janosh-") + user + ".ipc").c_str());
        subscriber.setsockopt(ZMQ_SUBSCRIBE, prefix.data(), prefix.length());
        LuaScript* parent = LuaScript::getInstance();

        while(true) {
          zmq::message_t update;
          LuaScript script(parent->openCallback_, parent->requestCallback_, parent->closeCallback_);
          script.loadString(("load(\"" + luaCode + "\")()").c_str());
          subscriber.recv(&update);
          script.run();
        }
      });
      t.detach();
}

using std::vector;

LuaScript* LuaScript::instance_ = NULL;

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

static janosh::Request make_request(lua_State* L) {
  std::vector< string > args;

  lua_pushinteger( L, 1 );
  lua_gettable( L, -2 );
  string command = lua_tostring( L, -1 );
  lua_pop( L, 1 );

  const int len = lua_objlen( L, -1 );
  for ( int i = 2; i <= len; ++i ) {
      lua_pushinteger( L, i );
      lua_gettable( L, -2 );
      args.push_back( lua_tostring( L, -1 ) );
      lua_pop( L, 1 );
  }

  return Request(janosh::Format::Json, command, args, {}, false, false, get_parent_info());
}

static janosh::Request make_request(string command, lua_State* L) {
  std::vector< string > args;

  const int len = lua_objlen( L, -1 );
  for ( int i = 1; i <= len; ++i ) {
      lua_pushinteger( L, i );
      lua_gettable( L, -2 );
      args.push_back( lua_tostring( L, -1 ) );
      lua_pop( L, 1 );
  }

  return Request(janosh::Format::Json, command, args, {}, false, false, get_parent_info());
}

static int l_sleep(lua_State* L) {
  std::this_thread::sleep_for(std::chrono::milliseconds(lua_tointeger( L, -1 )));
  return 0;
}

static int l_subscribe(lua_State* L) {
//  lua_gettop(L);
  string callback = lua_tostring( L, -1 );
  string prefix = lua_tostring( L, -2);
  make_subscription(prefix, callback);
  return 0;
}

static int l_installChangeCallback(lua_State* L) {
  LuaScript::getInstance()->setLuaChangeCallback(lua_tostring( L, -1 ));
  return 0;
}

static int l_poll(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("", L));
  return 1;
}

static int l_request(lua_State* L) {
  lua_pushstring(L, (LuaScript::getInstance()->performRequest(make_request(L))).c_str());
  return 1;
}

static int l_get(lua_State* L) {
  lua_pushstring(L, (LuaScript::getInstance()->performRequest(make_request("get", L))).c_str());
  return 1;
}

static int l_set(lua_State* L) {
  LuaScript::getInstance()->performRequest(make_request("set", L));
  return 0;
}

static int l_open(lua_State* L) {
  LuaScript::getInstance()->performOpen();
  return 0;
}

static int l_close(lua_State* L) {
  LuaScript::getInstance()->performClose();
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
 lua_pushstring(L, LuaScript::getInstance()->performRequest(make_request("dump", L)).c_str());
 return 1;
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

LuaScript::LuaScript(std::function<void()> openCallback,
    std::function<std::pair<string,string>(janosh::Request&)> requestCallback,
    std::function<void()> closeCallback) : openCallback_(openCallback), requestCallback_(requestCallback), closeCallback_(closeCallback), lastRevision_() {
  L = luaL_newstate();
  luaL_openlibs(L);
  lua_pushcfunction(L, l_subscribe);
  lua_setglobal(L, "janosh_subscribe");
  lua_pushcfunction(L, l_sleep);
  lua_setglobal(L, "janosh_sleep");
  lua_pushcfunction(L, l_poll);
  lua_setglobal(L, "janosh_poll");
  lua_pushcfunction(L, l_request);
  lua_setglobal(L, "janosh_request");
  lua_pushcfunction(L, l_set);
  lua_setglobal(L, "janosh_set");
  lua_pushcfunction(L, l_open);
  lua_setglobal(L, "janosh_open");
  lua_pushcfunction(L, l_close);
  lua_setglobal(L, "janosh_close");
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

void LuaScript::loadString(const std::string& luacode) {
  if (luaL_loadstring(L, luacode.c_str())) {
    LOG_ERR_MSG("Failed to load string", lua_tostring(L, -1));
  }
}

void LuaScript::run() {
  if(lua_pcall(L, 0, 0, 0)) {
    LOG_ERR_MSG("Lua script failed", lua_tostring(L, -1));
  }
}

void LuaScript::performOpen() {
  openCallback_();
  isOpen = true;
}

void LuaScript::performClose() {
  closeCallback_();
  isOpen = false;
}

string LuaScript::performRequest(janosh::Request req) {
  requestMutex_.lock();

  if(!isOpen) {
    performOpen();
    auto result = requestCallback_(req);
    performClose();
    if(result.first != lastRevision_ && !luaChangeCallbackName_.empty()) {
      lastRevision_ = result.first;
      lua_getglobal(L, luaChangeCallbackName_.c_str());
      lua_call(L, 0, 0);
    }
    requestMutex_.unlock();
    return result.second;
  } else {
    auto result = requestCallback_(req);
    if(result.first != lastRevision_) {
      lastRevision_ = result.first;
      lua_getglobal(L, luaChangeCallbackName_.c_str());
      lua_call(L, 0, 0);
    }
    requestMutex_.unlock();
    return result.second;
  }
}
}
}
