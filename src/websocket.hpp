#ifndef JANOSH_WEBSOCKET_HPP_
#define JANOSH_WEBSOCKET_HPP_

#include "websocketpp/config/asio_no_tls.hpp"
#include "websocketpp/server.hpp"
#include <iostream>
#include <set>
#include <deque>
#include <map>
#include <memory>

namespace janosh {
namespace lua {

using std::string;
typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using websocketpp::lib::thread;
using websocketpp::lib::mutex;
using websocketpp::lib::unique_lock;
using websocketpp::lib::condition_variable;

/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */

enum action_type {
    SUBSCRIBE,
    UNSUBSCRIBE,
    MESSAGE
};

struct action {
    action(action_type t, connection_hdl h) : type(t), hdl(h) {}
    action(action_type t, std::string m)
      : type(t), msg(m) {}

    action_type type;
    websocketpp::connection_hdl hdl;
    std::string msg;
};

typedef std::pair<size_t, std::string> lua_message;

struct Credentials {
  std::string hash;
  std::string salt;
  std::string userData;
};

typedef std::map<std::string, Credentials> AuthData;
typedef std::shared_ptr<void> ConnectionHandle;

class WebsocketServer {
private:
    WebsocketServer(const std::string passwdFile = "");
    ~WebsocketServer();
    string loginUser(const connection_hdl hdl, const std::string& sessionKey);
    string loginUser(const connection_hdl hdl, const std::string& username, const std::string& password);
    string registerUser(const connection_hdl hdl, const std::string& username, const std::string& password, const std::string& userdata);
    void readAuthData(const std::string& passwdFile);
    void run(uint16_t port);
    void on_open(connection_hdl hdl);
    void on_close(connection_hdl hdl);
    void on_message(connection_hdl hdl, server::message_ptr msg);
    void process_messages();
public:
    string getUserData(size_t luahandle);
    string getUserName(size_t luahandle);
    size_t getHandle(string username);

    void broadcast(const std::string& s);
    std::pair<size_t, std::string> receive();
    void send(size_t luahandle, const std::string& message);

    static void init(const int port, const string passwdFile = "");
    static WebsocketServer* getInstance();
private:
    typedef std::set<ConnectionHandle,std::owner_less<ConnectionHandle> > con_list;
    typedef std::map<size_t, ConnectionHandle > con_lua_handles;
    typedef std::map<ConnectionHandle, size_t, std::owner_less<ConnectionHandle> > con_lua_handles_rev;

    server m_server;
    con_list m_connections;
    con_lua_handles m_luahandles;
    con_lua_handles_rev m_luahandles_rev;
    size_t luaHandleMax = 0;
    std::queue<action> m_actions;

    mutex m_action_lock;
    mutex m_connection_lock;
    condition_variable m_action_cond;
    static WebsocketServer* server_instance;

    mutex m_receive_lock;
    condition_variable m_receive_cond;
    std::deque<lua_message> m_receive;

    bool doAuthenticate = false;
    std::map<string, string> usernameMap;
    std::map<string, ConnectionHandle> sessionMap;
    std::map<ConnectionHandle, string> sessionMapRev;


    AuthData authData;
    string passwdFile;
};

}
}


#endif
