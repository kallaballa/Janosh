#ifndef JANOSH_WEBSOCKET_HPP_
#define JANOSH_WEBSOCKET_HPP_

#include <uWS/uWS.h>
using namespace uWS;

#include <iostream>
#include <set>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include "semaphore.hpp"


namespace janosh {
namespace lua {
using std::mutex;
using std::condition_variable;
using std::string;


/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */

enum action_type {
  SUBSCRIBE, UNSUBSCRIBE, MESSAGE, BROADCAST
};

typedef WebSocket<SERVER>* connection_hdl;

struct action {
  action(action_type t, connection_hdl h) :
      type(t), hdl(h) {
  }
  action(action_type t, std::string m) :
      type(t), hdl(NULL), msg(m) {
  }

  action_type type;
  connection_hdl hdl;
  std::string msg;
};

typedef std::pair<size_t, std::string> lua_message;

struct Credentials {
  std::string hash;
  std::string salt;
  std::string userData;
};

typedef std::map<std::string, Credentials> AuthData;

class WebsocketServer {
private:
  class Authenticator {
  private:
    std::map<size_t, connection_hdl> m_luahandles;
    std::map<connection_hdl, size_t> m_luahandles_rev;
    size_t luaHandleMax = 0;
    std::map<string, string> skeyNameMap;
    std::multimap<string, string> nameSkeyMap;

    std::map<string, connection_hdl> skeyConMap;
    std::map<connection_hdl, string> conSkeyMap;
    AuthData authData;
    string passwdFile;
  public:
    Authenticator() {}
    ~Authenticator() {}
    bool hasUsername(const std::string& username);
    bool hasSession(const std::string& sessionKey);
    bool hasConnectionHandle(connection_hdl c);
    bool hasLuaHandle(size_t l);


    void remapSession(const connection_hdl h, const std::string& sessionKey);
    string createSession(const connection_hdl h, const std::string& username, const std::string& password);
    string createUser(const connection_hdl hdl, const std::string& username, const std::string& password, const std::string& userdata);
    void destroySession(const std::string& sessionKey);

    void readAuthData(const std::string& passwdFile);
    string getUserData(size_t luahandle);
    string getUserName(size_t luahandle);
    std::vector<size_t> getHandles(const string& username);
    size_t getLuaHandle(connection_hdl c);
    connection_hdl getConnectionHandle(size_t luaHandle);
    size_t createLuaHandle(connection_hdl c);
    void destroyLuaHandle(connection_hdl c);
  };


  WebsocketServer(const std::string passwdFile = "");
  ~WebsocketServer();
  string loginUser(const connection_hdl hdl, const std::string& sessionKey);
  string loginUser(const connection_hdl hdl, const std::string& username, const std::string& password);
  string registerUser(const connection_hdl hdl, const std::string& username, const std::string& password, const std::string& userdata);
  bool logoutUser(const string& sessionKey);

  void run(uint16_t port);
  void on_open(uWS::WebSocket<uWS::SERVER> *ws);
  void on_close(uWS::WebSocket<uWS::SERVER> *ws);
  void on_message(WebSocket<SERVER> *ws, char *message, size_t length, OpCode opCode);
  void process_messages();
public:
  string getUserData(size_t luahandle);
  string getUserName(size_t luahandle);
  std::vector<size_t> getHandles(const string& username);
  size_t getHandle(string username);

  void broadcast(const std::string& s);
  std::pair<size_t, std::string> receive();
  void send(size_t luahandle, const std::string& message);

  static void init(const int port, const string passwdFile = "");
  static WebsocketServer* getInstance();
private:
  typedef std::set<connection_hdl> con_list;

  Hub m_server;
  con_list m_connections;
  std::queue<action> m_actions;

  mutex m_action_lock;
  mutex m_connection_lock;
  condition_variable m_action_cond;
  static WebsocketServer* server_instance;

  mutex m_receive_lock;
  condition_variable m_receive_cond;
  std::deque<lua_message> m_receive;

  bool doAuthenticate = false;
  Authenticator auth;
  Semaphore messageSema;
};

}
}

#endif
