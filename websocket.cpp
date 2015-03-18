#include "websocket.hpp"
#include "websocketpp/endpoint.hpp"

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

broadcast_server::broadcast_server() {
  m_server.clear_access_channels(websocketpp::log::alevel::all);

  // Initialize Asio Transport
  m_server.init_asio();
  // Register handler callbacks
  m_server.set_open_handler(bind(&broadcast_server::on_open, this, ::_1));
  m_server.set_close_handler(bind(&broadcast_server::on_close, this, ::_1));
  m_server.set_message_handler(bind(&broadcast_server::on_message, this, ::_1, ::_2));
}

void broadcast_server::run(uint16_t port) {
  // listen on specified port
  m_server.listen(port);

  // Start the server accept loop
  m_server.start_accept();

  // Start the ASIO io_service run loop
  try {
    m_server.run();
  } catch (const std::exception & e) {
    std::cout << e.what() << std::endl;
  }
}

void broadcast_server::on_open(connection_hdl hdl) {
  unique_lock<mutex> lock(m_action_lock);
  //std::cout << "on_open" << std::endl;
  m_actions.push(action(SUBSCRIBE, hdl));
  lock.unlock();
  m_action_cond.notify_one();
}

void broadcast_server::on_close(connection_hdl hdl) {
  unique_lock<mutex> lock(m_action_lock);
  //std::cout << "on_close" << std::endl;
  m_actions.push(action(UNSUBSCRIBE, hdl));
  lock.unlock();
  m_action_cond.notify_one();
}

void broadcast_server::on_message(connection_hdl hdl, server::message_ptr msg) {
  unique_lock<mutex> lock(m_receive_lock);
  m_receive = std::make_pair(m_luahandles_rev[hdl], msg->get_payload());
  lock.unlock();
  m_receive_cond.notify_one();
}

void broadcast_server::process_messages() {
  while (1) {
    unique_lock<mutex> lock(m_action_lock);

    while (m_actions.empty()) {
      m_action_cond.wait(lock);
    }

    action a = m_actions.front();
    m_actions.pop();

    lock.unlock();

    if (a.type == SUBSCRIBE) {
      unique_lock<mutex> con_lock(m_connection_lock);
      m_connections.insert(a.hdl);
      m_luahandles[++luaHandleMax] = a.hdl;
      m_luahandles_rev[a.hdl] = luaHandleMax;
    } else if (a.type == UNSUBSCRIBE) {
      unique_lock<mutex> con_lock(m_connection_lock);
      m_connections.erase(a.hdl);
      size_t intHandle = m_luahandles_rev[a.hdl];
      m_luahandles.erase(intHandle);
      m_luahandles_rev.erase(a.hdl);
    } else if (a.type == MESSAGE) {
      unique_lock<mutex> con_lock(m_connection_lock);

      con_list::iterator it;
      for (it = m_connections.begin(); it != m_connections.end(); ++it) {
        m_server.send(*it, a.msg, websocketpp::frame::opcode::TEXT);
      }
    } else {
      assert(false);
    }
  }
}

void broadcast_server::broadcast(const std::string& s) {
  // queue message up for sending by processing thread
  unique_lock<mutex> lock(m_action_lock);
  //std::cout << "on_message" << std::endl;
  m_actions.push(action(MESSAGE, s));
  lock.unlock();
  m_action_cond.notify_one();
}

std::pair<size_t, std::string> broadcast_server::receive() {
  unique_lock<mutex> lock(m_receive_lock);
  m_receive_cond.wait(lock);
  return m_receive;
}

void broadcast_server::send(size_t handle, const std::string& message) {
  unique_lock<mutex> con_lock(m_connection_lock);
  auto it = m_luahandles.find(handle);
  if(it != m_luahandles.end())
    m_server.send((*it).second, message, websocketpp::frame::opcode::TEXT);
}

void broadcast_server::init(int port) {
  assert(server_instance == NULL);
  server_instance = new broadcast_server();
  thread t(bind(&broadcast_server::process_messages, server_instance));
  std::thread maint([=](){
    server_instance->run(port);
  });

  t.detach();
  maint.detach();
}

broadcast_server* broadcast_server::getInstance() {
  assert(server_instance != NULL);
  return server_instance;
}

broadcast_server* broadcast_server::server_instance = NULL;
}
}

