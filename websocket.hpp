#ifndef JANOSH_WEBSOCKET_HPP_
#define JANOSH_WEBSOCKET_HPP_

#include "websocketpp/config/asio_no_tls.hpp"
#include "websocketpp/server.hpp"
#include <iostream>
#include <set>

namespace janosh {
namespace lua {

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

class broadcast_server {
private:
    broadcast_server();
    void run(uint16_t port);
    void on_open(connection_hdl hdl);
    void on_close(connection_hdl hdl);
    void on_message(connection_hdl hdl, server::message_ptr msg);
    void process_messages();
public:
    void broadcast(const std::string& s);
    std::pair<size_t, std::string> receive();
    void send(size_t handle, const std::string& message);

    static void init(int port);
    static broadcast_server* getInstance();
private:
    typedef std::set<connection_hdl,std::owner_less<connection_hdl> > con_list;
    typedef std::map<size_t, connection_hdl > con_lua_handles;
    typedef std::map<connection_hdl, size_t, std::owner_less<connection_hdl> > con_lua_handles_rev;

    server m_server;
    con_list m_connections;
    con_lua_handles m_luahandles;
    con_lua_handles_rev m_luahandles_rev;
    size_t luaHandleMax = 0;
    std::queue<action> m_actions;

    mutex m_action_lock;
    mutex m_connection_lock;
    condition_variable m_action_cond;
    static broadcast_server* server_instance;

    mutex m_receive_lock;
    condition_variable m_receive_cond;
    lua_message m_receive;
};

}
}


#endif
