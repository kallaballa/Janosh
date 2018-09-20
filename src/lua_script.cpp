#include "lua_script.hpp"
#include <vector>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <chrono>
#include <thread>
#include <mutex>
#include <string>
#include "exception.hpp"
#include <signal.h>
#include <stdio.h>
#include "cppzmq/zmq.hpp"
#include "websocket.hpp"
#include "exception.hpp"

#ifndef JANOSH_NO_XDO
#include "xdo.hpp"
#endif

namespace janosh {
namespace lua {

using namespace janosh;

static int wrap_exceptions(lua_State *L, lua_CFunction f)
{
  string message;
  try {
    return f(L);  // Call wrapped function and return result.
  } catch (const char *s) {  // Catch and convert exceptions.
    message = s;
  } catch (janosh::janosh_exception& e) {
    std::stringstream ss;
    printException(e, ss);
    message = ss.str();
  } catch (std::exception& e) {
    message = e.what();
  } catch (...) {
    message = "...";
  }

  LOG_DEBUG_MSG("Caught message in lua call", message);
  lua_pushstring(L, message.c_str());
  return lua_error(L);  // Rethrow as a Lua error.
}

static std::map<string, std::mutex> lua_lock_map;
static std::mutex lua_lock_map_mutex;

class Subscriptions {
private:
  std::mutex mutex;
  std::map<string, zmq::context_t*> contextMap;
  std::map<string, zmq::socket_t*> socketMap;
public:
  void make(const string& prefix) {
    std::unique_lock<std::mutex> lock(mutex);
    if(contextMap.find(prefix) != contextMap.end())
      throw janosh_exception() << string_info( { "Attempt to create a duplicate subscription detected", prefix });

    zmq::context_t* context = new zmq::context_t(1);
    zmq::socket_t* subscriber = new zmq::socket_t(*context, ZMQ_SUB);
    string user = std::getenv("USER");
    LOG_DEBUG_STR("Connecting to: " + string("ipc:///tmp/janosh-") + user + ".ipc");
    subscriber->connect((string("ipc:///tmp/janosh-") + user + ".ipc").c_str());
    subscriber->setsockopt(ZMQ_SUBSCRIBE, prefix.data(), prefix.length());
    contextMap[prefix] = context;
    socketMap[prefix] = subscriber;
  }

  std::tuple<string,string,string> receive(const string& prefix) {
    if(contextMap.find(prefix) == contextMap.end())
      throw janosh_exception() << string_info( { "Attempt to read from an unknown subscription", prefix });

    zmq::message_t update;
    socketMap[prefix]->recv(&update);
    char* data = new char[update.size() + 1];
    memcpy(data, update.data(), update.size());
    data[update.size()] = 0;
    string s(data);
    delete[] data;
    size_t sep = s.find(' ');
    if(sep == std::string::npos)
      throw janosh_exception() << string_info({"Parsing key failed", s});

    string key = s.substr(0, sep);

    if(key.empty())
      throw janosh_exception() << string_info({"Key empty", s});

    if(s.size() < sep + 2)
      throw janosh_exception() << string_info({"Operation empty", s});

    size_t sep2 = s.find(' ', sep + 1);
    if(sep2 == std::string::npos)
      throw janosh_exception() << string_info({"Value separator missing", s});

    string op = s.substr(sep + 1, sep2 - (sep + 1));
    string value = s.substr(sep2 + 1, s.size() - (sep2 + 1));
    return std::make_tuple(key, op, value);
  }

  void destroy(const string& prefix) {
    std::unique_lock<std::mutex> lock(mutex);
    if(contextMap.find(prefix) == contextMap.end())
      throw janosh_exception() << string_info( { "Attempt to destroy an unknown subscription", prefix });

    zmq::context_t* context = contextMap[prefix];
    zmq::socket_t* subscriber = socketMap[prefix];
    contextMap.erase(prefix);
    socketMap.erase(prefix);
    subscriber->close();
    context->close();
    delete subscriber;
    delete context;
  }
};

Subscriptions subscriptions;

using std::vector;

LuaScript* LuaScript::instance_ = NULL;

//static inline std::string &ltrim(std::string &s) {
//        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
//        return s;
//}
//
//// trim from end
//static inline std::string &rtrim(std::string &s) {
//        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
//        return s;
//}
//
//// trim from both ends
//static inline std::string &trim(std::string &s) {
//        return ltrim(rtrim(s));
//}

bool is_number(const std::string& s) {
  return !s.empty() && std::find_if(s.begin(), s.end(), [](char c) {return c != '.' && !std::isdigit(c);}) == s.end();
}

static janosh::Request make_request(lua_State* L, bool trigger = false) {
  std::vector< string > args;

  lua_pushinteger( L, 1 );
  lua_gettable( L, -2 );
  string info = lua_tostring( L, -1 );
  lua_pop( L, 1 );

  lua_pushinteger( L, 2 );
  lua_gettable( L, -2 );
  string command = lua_tostring( L, -1 );
  lua_pop( L, 1 );

  const int len = lua_objlen( L, -1 );
  for ( int i = 3; i <= len; ++i ) {
      lua_pushinteger( L, i );
      lua_gettable( L, -2 );
      if(lua_type(L, -1) == LUA_TSTRING)
        args.push_back( lua_tostring( L, -1 ) );
      if(lua_type(L, -1) == LUA_TBOOLEAN)
        args.push_back( lua_toboolean( L, -1 ) > 0 ? "true" : "false" );
      else if(lua_type(L, -1) == LUA_TNUMBER)
        args.push_back( std::to_string(lua_tonumber( L, -1 )) );

      lua_pop( L, 1 );
  }
  std::vector<Value> typedArgs;
  for(auto& arg :args) {
    if(arg.empty() || arg.at(0) == '"')  {
      if(!arg.empty())
        arg = arg.substr(1, arg.size() -2);
      typedArgs.push_back(Value{arg, Value::String});
    } else {
      if(is_number(arg)) {
        typedArgs.push_back(Value{arg, Value::Number});
      } else if(arg == "true" || arg == "false") {
        typedArgs.push_back(Value{arg, Value::Boolean});
      } else {
        typedArgs.push_back(Value{arg, Value::String});
      }
    }
  }

  return Request(janosh::Format::Json, command, typedArgs, trigger, false, get_parent_info(), info);
}

//static janosh::Request make_request(string command, lua_State* L, bool trigger = false) {
//  std::vector< string > args;
//
//  const int len = lua_objlen( L, -1 );
//  for ( int i = 1; i <= len; ++i ) {
//      lua_pushinteger( L, i );
//      lua_gettable( L, -2 );
//      args.push_back( lua_tostring( L, -1 ) );
//      lua_pop( L, 1 );
//  }
//
//  return Request(janosh::Format::Json, command, args, trigger, false, get_parent_info());
//}

static int l_register_thread(lua_State* L) {
  string name = lua_tostring(L, -1);
  janosh::Logger::getInstance().registerThread(name);
  return 0;
}

static int l_subscribe(lua_State* L) {
  string prefix = lua_tostring(L, -1);
  subscriptions.make(prefix);
  return 0;
}

static int l_receive(lua_State* L) {
  string prefix = lua_tostring(L, -1);
  auto res = subscriptions.receive(prefix);
  lua_pushstring(L, std::get<0>(res).c_str());
  lua_pushstring(L, std::get<1>(res).c_str());
  lua_pushstring(L, std::get<2>(res).c_str());
  return 3;
}

static int l_try_lock(lua_State* L) {
  std::unique_lock<std::mutex> lock(lua_lock_map_mutex);
  string name = lua_tostring(L, -1);
  bool result = lua_lock_map[name].try_lock();
  lua_pushboolean(L, result);
  return 1;
}

static int l_lock(lua_State* L) {
  std::unique_lock<std::mutex> lock(lua_lock_map_mutex);
  string name = lua_tostring(L, -1);
  lua_lock_map[name].lock();
  return 0;
}

static int l_unlock(lua_State* L) {
  std::unique_lock<std::mutex> lock(lua_lock_map_mutex);
  string name = lua_tostring(L, -1);
  auto it = lua_lock_map.find(name);
  if(it == lua_lock_map.end())
    throw janosh_exception() << string_info({"Attempt to unlock unknown lock", name});

  (*it).second.unlock();
  return 0;
}

static int l_sleep(lua_State* L) {
  std::this_thread::sleep_for(std::chrono::milliseconds(lua_tointeger( L, -1 )));
  return 0;
}

static int l_wsopen(lua_State* L) {
  //FIXME race condition
  if (lua_gettop(L) == 1) {
    WebsocketServer::init(lua_tointeger(L, -1));
  } else if (lua_gettop(L) > 1) {
    WebsocketServer::init(lua_tointeger(L, -2), lua_tostring(L, -1));
  }
  return 0;
}

static int l_wsbroadcast(lua_State* L) {
  WebsocketServer::getInstance()->broadcast(string(lua_tostring(L, -1)));
  return 0;
}

static int l_wsreceive(lua_State* L) {
  auto message = WebsocketServer::getInstance()->receive();
  lua_pushinteger(L, message.first);
  lua_pushstring(L, message.second.c_str());
  return 2;
}

static int l_wssend(lua_State* L) {
  string message = lua_tostring( L, -1 );
  size_t handle = lua_tointeger( L, -2);
  WebsocketServer::getInstance()->send(handle,message);
  return 0;
}

static int l_wsgetuserdata(lua_State* L) {
  size_t handle  = lua_tointeger(L, -1);
  string userdata = WebsocketServer::getInstance()->getUserData(handle);
  lua_pushstring(L, userdata.c_str());
  return 1;
}

static int l_wsgetusername(lua_State* L) {
  size_t handle  = lua_tointeger(L, -1);
  string username = WebsocketServer::getInstance()->getUserName(handle);
  lua_pushstring(L, username.c_str());
  return 1;
}

static int l_request(lua_State* L) {
  auto result = LuaScript::getInstance()->performRequest(make_request(L));

  lua_pushnumber(L, result.first);
  lua_pushstring(L, result.second.c_str());

  return 2;
}

static int l_request_trigger(lua_State* L) {
  auto result = LuaScript::getInstance()->performRequest(make_request(L, true));

  lua_pushnumber(L, result.first);
  lua_pushstring(L, result.second.c_str());

  return 2;
}

static int l_raw(lua_State* L) {
  string key = lua_tostring(L, -1);
  Request req(janosh::Format::Raw, "get", {{key, Value::String}}, false, false, get_parent_info(), "");
  auto result = LuaScript::getInstance()->performRequest(req);
  lua_pushnumber(L, result.first);
  lua_pushstring(L, result.second.c_str());
  return 2;
}

static int l_open(lua_State* L) {
  string id = lua_tostring(L, -1);
  LuaScript::getInstance()->performOpen(id);
  return 0;
}

static int l_close(lua_State* L) {
  LuaScript::getInstance()->performClose();
  return 0;
}

static int l_mouse_move(lua_State* L) {
  float y = boost::lexical_cast<float>(lua_tostring( L, -1 ));
  float x = boost::lexical_cast<float>(lua_tostring( L, -2));
#ifndef JANOSH_NO_XDO
  auto size = XDO::getInstance()->getScreenSize();
  XDO::getInstance()->mouseMove(x * size.first,y * size.second);
#else
  LOG_DEBUG_STR("Compiled without XDO support");
#endif

  return 0;
}

static int l_mouse_move_rel(lua_State* L) {
  int y = lua_tointeger( L, -1 );
  int x = lua_tointeger( L, -2);
#ifndef JANOSH_NO_XDO
  XDO::getInstance()->mouseMoveRelative(x,y);
#else
  LOG_DEBUG_STR("Compiled without XDO support");
#endif

  return 0;
}

static int l_mouse_down(lua_State* L) {
  size_t button = lua_tointeger( L, -1);
#ifndef JANOSH_NO_XDO
  XDO::getInstance()->mouseDown(button);
#else
  LOG_DEBUG_STR("Compiled without xdo support");
#endif

  return 0;
}

static int l_mouse_up(lua_State* L) {
  size_t button = lua_tointeger( L, -1);
#ifndef JANOSH_NO_XDO
  XDO::getInstance()->mouseUp(button);
#else
  LOG_DEBUG_STR("Compiled without XDO support");
#endif

  return 0;
}

static int l_key_down(lua_State* L) {
  string keySym = lua_tostring( L, -1);

#ifndef JANOSH_NO_XDO
  XDO::getInstance()->keyDown(keySym);
#else
  LOG_DEBUG_STR("Compiled without XDO support");
#endif

  return 0;
}

static int l_key_up(lua_State* L) {
  string keySym = lua_tostring( L, -1);
#ifndef JANOSH_NO_XDO
  XDO::getInstance()->keyUp(keySym);
#else
  LOG_DEBUG_STR("Compiled without XDO support");
#endif

  return 0;
}

static int l_key_type(lua_State* L) {
  string keySym = lua_tostring( L, -1);
#ifndef JANOSH_NO_XDO
  XDO::getInstance()->keyType(keySym);
#else
  LOG_DEBUG_STR("Compiled without XDO support");
#endif

  return 0;
}

static void install_janosh_functions(lua_State* L, bool first);
static int l_install(lua_State* L) {
  install_janosh_functions(L, false);
  return 0;
}

static void install_janosh_functions(lua_State* L, bool first) {
  lua_pushlightuserdata(L, (void *)wrap_exceptions);
  luaJIT_setmode(L, -1, LUAJIT_MODE_WRAPCFUNC|LUAJIT_MODE_ON);
  lua_pop(L, 1);

  luaL_openlibs(L);
  lua_pushcfunction(L, l_install);
  lua_setglobal(L, "janosh_install");
  lua_pushcfunction(L, l_register_thread);
  lua_setglobal(L, "janosh_register_thread");

  lua_pushcfunction(L, l_mouse_move);
  lua_setglobal(L, "janosh_mouse_move");
  lua_pushcfunction(L, l_mouse_move_rel);
  lua_setglobal(L, "janosh_mouse_move_rel");
  lua_pushcfunction(L, l_mouse_up);
  lua_setglobal(L, "janosh_mouse_up");
  lua_pushcfunction(L, l_mouse_down);
  lua_setglobal(L, "janosh_mouse_down");
  lua_pushcfunction(L, l_key_down);
  lua_setglobal(L, "janosh_key_down");
  lua_pushcfunction(L, l_key_up);
  lua_setglobal(L, "janosh_key_up");
  lua_pushcfunction(L, l_key_type);
  lua_setglobal(L, "janosh_key_type");

  lua_pushcfunction(L, l_try_lock);
  lua_setglobal(L, "janosh_try_lock");
  lua_pushcfunction(L, l_lock);
  lua_setglobal(L, "janosh_lock");
  lua_pushcfunction(L, l_unlock);
  lua_setglobal(L, "janosh_unlock");

  lua_pushcfunction(L, l_open);
  lua_setglobal(L, "janosh_open");
  lua_pushcfunction(L, l_raw);
  lua_setglobal(L, "janosh_raw");
  lua_pushcfunction(L, l_close);
  lua_setglobal(L, "janosh_close");

  lua_pushcfunction(L, l_wsopen);
  lua_setglobal(L, "janosh_wsopen");
  lua_pushcfunction(L, l_wsbroadcast);
  lua_setglobal(L, "janosh_wsbroadcast");
  lua_pushcfunction(L, l_wsreceive);
  lua_setglobal(L, "janosh_wsreceive");
  lua_pushcfunction(L, l_wssend);
  lua_setglobal(L, "janosh_wssend");

  lua_pushcfunction(L, l_wsgetuserdata);
  lua_setglobal(L, "janosh_wsgetuserdata");
  lua_pushcfunction(L, l_wsgetusername);
  lua_setglobal(L, "janosh_wsgetusername");

  lua_pushcfunction(L, l_subscribe);
  lua_setglobal(L, "janosh_subscribe");
  lua_pushcfunction(L, l_receive);
  lua_setglobal(L, "janosh_receive");


  lua_pushcfunction(L, l_sleep);
  lua_setglobal(L, "janosh_sleep");

  lua_pushcfunction(L, l_request);
  lua_setglobal(L, "janosh_request");
  lua_pushcfunction(L, l_request_trigger);
  lua_setglobal(L, "janosh_request_t");

  // Load the Web.lua, set it to the Web table
  lua_getglobal(L, "require");
  lua_pushliteral(L, "Web");
  if(lua_pcall(L, 1, 1, 0)) {
    LOG_ERR_MSG("Preloading JSON library failed", lua_tostring(L, -1));
  }
  lua_setglobal(L, "Web");

  // Load the JSONLib, set it to the JSON table
  lua_getglobal(L, "require");
  lua_pushliteral(L, "JSONLib");
  if(lua_pcall(L, 1, 1, 0)) {
    LOG_ERR_MSG("Preloading JSON library failed", lua_tostring(L, -1));
  }
  lua_setglobal(L, "JSON");

  // set the JanoshFirstStart flag
  lua_pushboolean(L, first);
  lua_setglobal(L, "__JanoshFirstStart");

  // Load the JanoshAPI, set it to Janosh
  lua_getglobal(L, "require");
  lua_pushliteral(L, "JanoshAPI");
  if(lua_pcall(L, 1, 1, 0)) {
    LOG_ERR_MSG("Preloading JanoshAPI library failed", lua_tostring(L, -1));
  }
  lua_setglobal(L, "Janosh");

}

void printTransactionsHandler(int signum) {
  LuaScript::getInstance()->printTransactions(std::cerr);
}

LuaScript::LuaScript(std::function<void()> openCallback,
    std::function<std::pair<int,string>(janosh::Request&)> requestCallback,
    std::function<void()> closeCallback, lua_State* l) : openCallback_(openCallback), requestCallback_(requestCallback), closeCallback_(closeCallback) {
  if(l == NULL) {
    L = luaL_newstate();
    install_janosh_functions(L, true);
  }
  else {
    L = l;
  }
  clean();
  assert(signal(SIGUSR1, printTransactionsHandler) != SIG_ERR);
}

LuaScript::~LuaScript() {
	if(L) lua_close(L);
}

void LuaScript::defineMacros(const std::vector<std::pair<string,string>>& macros) {
  for(auto& p : macros) {
    this->makeGlobalVariable(p.first, p.second);
  }
}

void LuaScript::makeGlobalVariable(const string& key, const string& value) {
  lua_pushstring(L, value.c_str());
  lua_setglobal(L, key.c_str());
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
    int error = lua_pcall(L, 0, LUA_MULTRET, 0);

    if (error)
    {
      std::stringstream ss;
      size_t numMsg =lua_gettop(L);
      for(size_t i = 0; i < numMsg;++i) {
        ss << lua_tostring(L, -1) << std::endl;
        lua_pop(L, 1);
      }
      LOG_ERR_MSG("Lua script failed", ss.str());
    }
}

void LuaScript::clean() {
  lua_settop(L, 0);
}

//#define __JANOSH_DEBUG_QUEUE__
void LuaScript::performOpen(const string& strID, bool lockRequest) {
  if(lockRequest) {
    std::unique_lock<std::mutex> lock(open_lock_);
    bool wait = false;
#ifdef __JANOSH_DEBUG_QUEUE__
    std::cerr << "### openlock: " << std::this_thread::get_id() << std::endl;
    std::cerr << " ### queue: " << open_queue_.size() << std::endl;
#endif
    while(!open_queue_.empty() || isOpen) {
      if(!wait) {
#ifdef __JANOSH_DEBUG_QUEUE__
        std::cerr << " ### queued: " << std::this_thread::get_id() << std::endl;
#endif
        open_queue_.push_back({std::this_thread::get_id(), strID});
      }
      if(isOpen || wait)
        open_lock_cond_.wait(lock);

      if(std::this_thread::get_id() == open_queue_.front().first && !isOpen) {
#ifdef __JANOSH_DEBUG_QUEUE__
        std::cerr << " ### released: " << std::this_thread::get_id() << std::endl;
#endif
        break;
      } else {
#ifdef __JANOSH_DEBUG_QUEUE__
        std::cerr << " ### wait: " << std::this_thread::get_id() << std::endl;
#endif
        wait = true;
      }
    }
    if(open_queue_.empty())
      open_queue_.push_back({std::this_thread::get_id(),strID});
    openCallback_();
    isOpen = true;
    open_current_id = std::this_thread::get_id();
#ifdef __JANOSH_DEBUG_QUEUE__
    std::cerr << "### openlockend: " << std::this_thread::get_id() << std::endl;
#endif
  } else {
#ifdef __JANOSH_DEBUG_QUEUE__
    std::cerr << "### open: " << std::this_thread::get_id() << std::endl;
#endif
    openCallback_();
    isOpen = true;
    open_current_id = std::this_thread::get_id();
#ifdef __JANOSH_DEBUG_QUEUE__
    std::cerr << "### openend: " << std::this_thread::get_id() << std::endl;
#endif
  }
}

void LuaScript::performClose(bool lockRequest) {
  if(lockRequest) {
    std::unique_lock<std::mutex> lock(open_lock_);
#ifdef __JANOSH_DEBUG_QUEUE__
    std::cerr << "### closelock: " << std::this_thread::get_id() << std::endl;
#endif
    if(!isOpen)
      throw janosh_exception() << string_info({"Attempt to close and request that isn't open"});

    closeCallback_();
    isOpen = false;
    if(!open_queue_.empty())
      open_queue_.pop_front();

#ifdef __JANOSH_DEBUG_QUEUE__
    std::cerr << "### closelockend: " << std::this_thread::get_id() << std::endl;
#endif
    lock.unlock();
    open_lock_cond_.notify_all();
    std::this_thread::yield();
  } else {
#ifdef __JANOSH_DEBUG_QUEUE__
    std::cerr << "### close: " << std::this_thread::get_id() << std::endl;
#endif
    if(!isOpen)
      throw janosh_exception() << string_info({"Attempt to close and request that isn't open"});
    closeCallback_();
    isOpen = false;
#ifdef __JANOSH_DEBUG_QUEUE__
    std::cerr << "### closeend: " << std::this_thread::get_id() << std::endl;
#endif
  }

}

std::pair<int, string> LuaScript::performRequest(janosh::Request req) {
  std::unique_lock<std::mutex> lock(open_lock_);
#ifdef __JANOSH_DEBUG_QUEUE__
  std::cerr << " ### req: " << std::this_thread::get_id() << std::endl;
#endif
   if(!isOpen) {
    performOpen(req.info_, false);
    auto result = requestCallback_(req);
    performClose(false);
#ifdef __JANOSH_DEBUG_QUEUE__
    std::cerr << " ### reqend: " << std::this_thread::get_id() << std::endl;
#endif
    return result;
  } else {
    auto result = requestCallback_(req);
#ifdef __JANOSH_DEBUG_QUEUE__
    std::cerr << " ### reqend: " << std::this_thread::get_id() << std::endl;
#endif
    return result;
  }
}

void LuaScript::printTransactions(std::ostream& os) {
  std::unique_lock<std::mutex> lock(open_lock_);
  for(auto& p : open_queue_) {
    os << "Thread(" << p.first << "):" << p.second << std::endl;
  }
}
}
}
