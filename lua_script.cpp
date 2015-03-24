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

static int wrap_exceptions(lua_State *L, lua_CFunction f)
{
  try {
    return f(L);  // Call wrapped function and return result.
  } catch (const char *s) {  // Catch and convert exceptions.
    lua_pushstring(L, s);
  } catch (std::exception& e) {
    lua_pushstring(L, e.what());
  } catch (...) {
    lua_pushliteral(L, "caught (...)");
  }
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
      throw janosh_exception() << string_info({"Protocol exception", s});

    string key = s.substr(0, sep);
    if(s.size() < sep + 2)
      throw janosh_exception() << string_info({"Protocol exception", s});

    string op = string() + s[sep + 1];
    if(!(s[sep + 1] == 'W' || s[sep + 1] == 'D'))
      throw janosh_exception() << string_info({"Protocol exception", s});

    string value = s.substr(sep + 2, s.size());
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
  WebsocketServer::init(lua_tointeger(L, -1));
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

  lua_pushcfunction(L, l_lock);
  lua_setglobal(L, "janosh_lock");
  lua_pushcfunction(L, l_unlock);
  lua_setglobal(L, "janosh_unlock");

  lua_pushcfunction(L, l_wsopen);
  lua_setglobal(L, "janosh_wsopen");
  lua_pushcfunction(L, l_wsbroadcast);
  lua_setglobal(L, "janosh_wsbroadcast");
  lua_pushcfunction(L, l_wsreceive);
  lua_setglobal(L, "janosh_wsreceive");
  lua_pushcfunction(L, l_wssend);
  lua_setglobal(L, "janosh_wssend");

  lua_pushcfunction(L, l_subscribe);
  lua_setglobal(L, "janosh_subscribe");
  lua_pushcfunction(L, l_receive);
  lua_setglobal(L, "janosh_receive");

  lua_pushcfunction(L, l_sleep);
  lua_setglobal(L, "janosh_sleep");

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
  lua_pushboolean(L, first);
  lua_setglobal(L, "__JanoshFirstStart");
  luaL_loadstring(L, ss.str().c_str());
  if(lua_pcall(L, 0, 1, 0)) {
    LOG_ERR_MSG("Preloading JanoshAPI library failed", lua_tostring(L, -1));
  }
  lua_setglobal(L, "Janosh");

}

LuaScript::LuaScript(std::function<void()> openCallback,
    std::function<std::pair<string,string>(janosh::Request&)> requestCallback,
    std::function<void()> closeCallback, lua_State* l) : openCallback_(openCallback), requestCallback_(requestCallback), closeCallback_(closeCallback) {
  if(l == NULL) {
    L = luaL_newstate();
    install_janosh_functions(L, true);
  }
  else {
    L = l;
  }
  clean();
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

//#define __JANOSH_DEBUG_QUEUE__
void LuaScript::performOpen(bool lockRequest) {
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
        open_queue_.push(std::this_thread::get_id());
      }
      if(isOpen || wait)
        open_lock_cond_.wait(lock);

      if(std::this_thread::get_id() == open_queue_.front() && !isOpen) {
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
      open_queue_.push(std::this_thread::get_id());
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
      open_queue_.pop();

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

string LuaScript::performRequest(janosh::Request req) {
  std::unique_lock<std::mutex> lock(open_lock_);
#ifdef __JANOSH_DEBUG_QUEUE__
  std::cerr << " ### req: " << std::this_thread::get_id() << std::endl;
#endif
   if(!isOpen) {
    performOpen(false);
    auto result = requestCallback_(req);
    performClose(false);
#ifdef __JANOSH_DEBUG_QUEUE__
    std::cerr << " ### reqend: " << std::this_thread::get_id() << std::endl;
#endif
    return result.second;
  } else {
    auto result = requestCallback_(req);
#ifdef __JANOSH_DEBUG_QUEUE__
    std::cerr << " ### reqend: " << std::this_thread::get_id() << std::endl;
#endif
    return result.second;
  }
}
}
}
