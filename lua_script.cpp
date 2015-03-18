#include "lua_script.hpp"
#include <vector>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <chrono>
#include <thread>
#include <mutex>
#include <thread>
#include "cppzmq/zmq.hpp"
#include "websocket.hpp"

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
    subscriber.connect((string("ipc:///tmp/janosh-") + user + ".ipc").c_str());
    subscriber.setsockopt(ZMQ_SUBSCRIBE, prefix.data(), prefix.length());
    LuaScript* parent = LuaScript::getInstance();
    lua_State* Lchild = lua_newthread(parent->L);
    LuaScript script(parent->openCallback_, parent->requestCallback_, parent->closeCallback_, Lchild);

    string wrapped = "load(\"" + luaCode + "\")(...)";
    script.loadString(wrapped.c_str());
    lua_pushvalue(script.L, -1);
    int ref = luaL_ref(script.L, LUA_REGISTRYINDEX);
    LOG_DEBUG_MSG("Installed subscription", prefix);
    while(true) {
      zmq::message_t update;
      subscriber.recv(&update);
      char* data = new char[update.size() + 1];
      memcpy(data, update.data(), update.size());
      data[update.size()] = 0;
      string s(data);
      size_t sep = s.find(' ');
      string key = s.substr(0, sep);
      string value = s.substr(sep + 1, s.size());
      LOG_DEBUG_MSG("Running subscription", key);

      lua_pushstring(script.L, key.data());
      lua_pushstring(script.L, value.data());
      if(lua_pcall(script.L, 2, 0, 0)) {
        LOG_ERR_MSG("Lua subscribe failed", lua_tostring(script.L, -1));
      }
      script.clean();
      lua_rawgeti(script.L, LUA_REGISTRYINDEX, ref);
    }
  });
  t.detach();
}

void make_receiver(string luaCode) {
  std::thread t([=]() {
    LuaScript* parent = LuaScript::getInstance();
    lua_State* Lchild = lua_newthread(parent->L);
    LuaScript script(parent->openCallback_, parent->requestCallback_, parent->closeCallback_, Lchild);
    string wrapped = "load(\"" + luaCode + "\")(...)";
    script.loadString(wrapped.c_str());
    lua_pushvalue(script.L, -1);
    int ref = luaL_ref(script.L, LUA_REGISTRYINDEX);
    LOG_DEBUG_STR("Installed receiver");
    while(true) {
      auto message = broadcast_server::getInstance()->receive();
      LOG_DEBUG_MSG("Running receiver", message.second);
      lua_pushinteger(script.L, message.first);
      lua_pushstring(script.L, message.second.c_str());

      if(lua_pcall(script.L, 2, 0, 0)) {
        LOG_ERR_MSG("Lua subscribe failed", lua_tostring(script.L, -1));
      }
      script.clean();
      lua_rawgeti(script.L, LUA_REGISTRYINDEX, ref);
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

static int l_wsopen(lua_State* L) {
  //FIXME race condition
  broadcast_server::init(lua_tointeger(L, -1));
  return 0;
}

static int l_wsbroadcast(lua_State* L) {
  broadcast_server::getInstance()->broadcast(string(lua_tostring(L, -1)));
  return 0;
}

static int l_wsonreceive(lua_State* L) {
  make_receiver(lua_tostring( L, -1 ));
  return 0;
}

static int l_wssend(lua_State* L) {
  string message = lua_tostring( L, -1 );
  size_t handle = lua_tointeger( L, -2);
  broadcast_server::getInstance()->send(handle,message);
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
  lua_pushstring(L, (LuaScript::getInstance()->performRequest(make_request("set", L))).c_str());
  return 1;
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
    std::function<void()> closeCallback, lua_State* l) : openCallback_(openCallback), requestCallback_(requestCallback), closeCallback_(closeCallback), lastRevision_() {
  if(l == NULL)
    L = luaL_newstate();
  else
    L = l;
  luaL_openlibs(L);

  lua_pushcfunction(L, l_wsopen);
  lua_setglobal(L, "janosh_wsopen");
  lua_pushcfunction(L, l_wsbroadcast);
  lua_setglobal(L, "janosh_wsbroadcast");
  lua_pushcfunction(L, l_wsonreceive);
  lua_setglobal(L, "janosh_wsonreceive");
  lua_pushcfunction(L, l_wssend);
  lua_setglobal(L, "janosh_wssend");

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

void LuaScript::clean() {
  lua_settop(L, 0);
}

void LuaScript::performOpen(bool lockRequest) {
  if(lockRequest) {
    std::unique_lock<std::mutex> lock(open_lock_);
    bool wait = false;
    bool fromQueue = false;
    while(!open_queue_.empty() || isOpen) {
      if(!wait)
        open_queue_.push(std::this_thread::get_id());
      if(isOpen || wait)
        open_lock_cond_.wait(lock);
      if(std::this_thread::get_id() == open_queue_.front()) {
        fromQueue = true;
        break;
      } else {
        wait = true;
      }
    }
    if(fromQueue)
      open_queue_.pop();

    openCallback_();
    isOpen = true;
  } else {
    openCallback_();
    isOpen = true;
  }
}

void LuaScript::performClose(bool lockRequest) {
  if(lockRequest) {
    std::unique_lock<std::mutex> lock(open_lock_);
    if(!isOpen) {
      lock.unlock();
      open_lock_cond_.notify_all();
      return;
    }
    closeCallback_();
    isOpen = false;
    lock.unlock();
    open_lock_cond_.notify_all();
  } else {
    if(!isOpen)
      return;
    closeCallback_();
    isOpen = false;
  }
}

string LuaScript::performRequest(janosh::Request req) {
  std::unique_lock<std::mutex> lock(open_lock_);

  if(!isOpen) {
    performOpen(false);
    auto result = requestCallback_(req);
    performClose(false);
    return result.second;
  } else {
    auto result = requestCallback_(req);
    return result.second;
  }
}
}
}
