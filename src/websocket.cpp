#include "websocket.hpp"
#include "websocketpp/endpoint.hpp"
#include "exithandler.hpp"
#include "logger.hpp"

namespace janosh {
namespace lua {
typedef websocketpp::config::asio::message_type::ptr message_ptr;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

typedef websocketpp::server<websocketpp::config::asio_tls> server;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using websocketpp::lib::thread;
using websocketpp::lib::mutex;
using websocketpp::lib::unique_lock;
using websocketpp::lib::condition_variable;
enum tls_mode {
    MOZILLA_INTERMEDIATE = 1,
    MOZILLA_MODERN = 2
};

std::string get_password() {
    return "test";
}

context_ptr on_tls_init(tls_mode mode, websocketpp::connection_hdl hdl) {
    namespace asio = websocketpp::lib::asio;

    std::cout << "on_tls_init called with hdl: " << hdl.lock().get() << std::endl;
    std::cout << "using TLS mode: " << (mode == MOZILLA_MODERN ? "Mozilla Modern" : "Mozilla Intermediate") << std::endl;

    context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);

    try {
        if (mode == MOZILLA_MODERN) {
            // Modern disables TLSv1
            ctx->set_options(asio::ssl::context::default_workarounds |
                             asio::ssl::context::no_sslv2 |
                             asio::ssl::context::no_sslv3 |
                             asio::ssl::context::no_tlsv1 |
                             asio::ssl::context::single_dh_use);
        } else {
            ctx->set_options(asio::ssl::context::default_workarounds |
                             asio::ssl::context::no_sslv2 |
                             asio::ssl::context::no_sslv3 |
                             asio::ssl::context::single_dh_use);
        }
        ctx->set_password_callback(bind(&get_password));
        ctx->use_certificate_chain_file("server.crt");
        ctx->use_private_key_file("server.key", boost::asio::ssl::context::pem);


        // Example method of generating this file:
        // `openssl dhparam -out dh.pem 2048`
        // Mozilla Intermediate suggests 1024 as the minimum size to use
        // Mozilla Modern suggests 2048 as the minimum size to use.
        ctx->use_tmp_dh_file("dh512.pem");

        std::string ciphers;

        if (mode == MOZILLA_MODERN) {
            ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK";
        } else {
            ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:AES:CAMELLIA:DES-CBC3-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!MD5:!PSK:!aECDH:!EDH-DSS-DES-CBC3-SHA:!EDH-RSA-DES-CBC3-SHA:!KRB5-DES-CBC3-SHA";
        }

        if (SSL_CTX_set_cipher_list(ctx->native_handle() , ciphers.c_str()) != 1) {
            std::cout << "Error setting cipher list" << std::endl;
        }
    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
    return ctx;
}

WebsocketServer::WebsocketServer() {
  LOG_DEBUG_STR("Websocket: Init");

  m_server.set_access_channels(websocketpp::log::alevel::none);
  m_server.set_error_channels(websocketpp::log::elevel::all);
  // Initialize Asio Transport
  m_server.init_asio();

  // Register handler callbacks
  m_server.set_open_handler(bind(&WebsocketServer::on_open, this, std::placeholders::_1));
  m_server.set_close_handler(bind(&WebsocketServer::on_close, this, std::placeholders::_1));
  m_server.set_message_handler(bind(&WebsocketServer::on_message, this, std::placeholders::_1, std::placeholders::_2));
  m_server.set_tls_init_handler(bind(&on_tls_init,MOZILLA_INTERMEDIATE,std::placeholders::_1));

  ExitHandler::getInstance()->addExitFunc([&](){
    LOG_DEBUG_STR("Shutdown");
    if (m_server.is_listening()) {
      m_server.stop_perpetual();
      m_server.stop_listening();
    }
    LOG_DEBUG_STR("Stopped listening");

    for (auto& conn : m_connections) {
      m_server.close(conn, websocketpp::close::status::normal, "closed");
    }
    m_server.stop();
    LOG_DEBUG_STR("Stopped");
  });
  LOG_DEBUG_STR("Websocket: Init end");
}

WebsocketServer::~WebsocketServer() {
}
void WebsocketServer::run(uint16_t port) {
  LOG_DEBUG_STR("Websocket: Listen");

  // listen on specified port
  m_server.listen(boost::asio::ip::tcp::v4(),port);

  // Start the server accept loop
  m_server.start_accept();

  // Start the ASIO io_service run loop
  try {
    LOG_DEBUG_STR("Websocket: Run");
    m_server.run();
    LOG_DEBUG_STR("Websocket: End");
  } catch (const std::exception & e) {
    throw e;
  }
  LOG_DEBUG_STR("Websocket: End");
}

void WebsocketServer::on_open(connection_hdl hdl) {
  LOG_DEBUG_STR("Websocket: Open");
  unique_lock<mutex> lock(m_action_lock);
  m_actions.push(action(SUBSCRIBE, hdl));
  lock.unlock();
  m_action_cond.notify_one();
  LOG_DEBUG_STR("Websocket: Open end");
}

void WebsocketServer::on_close(connection_hdl hdl) {
  LOG_DEBUG_STR("Websocket: On close");
  unique_lock<mutex> lock(m_action_lock);
  m_actions.push(action(UNSUBSCRIBE, hdl));
  lock.unlock();
  m_action_cond.notify_one();
  LOG_DEBUG_STR("Websocket: On close end");
}

void WebsocketServer::on_message(connection_hdl hdl, server::message_ptr msg) {
  LOG_DEBUG_STR("Websocket: On message");
  unique_lock<mutex> lock(m_receive_lock);
  LOG_DEBUG_STR("Websocket: On message lock");
  if(m_luahandles_rev.find(hdl) != m_luahandles_rev.end())
    m_receive.push_back(std::make_pair(m_luahandles_rev[hdl], msg->get_payload()));
  lock.unlock();
  m_receive_cond.notify_one();
  LOG_DEBUG_STR("Websocket: On message end");
}

void WebsocketServer::process_messages() {
  while (1) {
    LOG_DEBUG_STR("Websocket: Process message");
    unique_lock<mutex> lock(m_action_lock);
    LOG_DEBUG_STR("Websocket: Process lock");

    try {
      LOG_DEBUG_STR("Websocket: Process action");

      while (m_actions.empty()) {
        m_action_cond.wait(lock);
      }
      LOG_DEBUG_STR("Websocket: Process action lock");
      action a = m_actions.front();
      m_actions.pop();

      lock.unlock();

      if (a.type == SUBSCRIBE) {
        LOG_DEBUG_STR("Websocket: Process message lock");
        unique_lock<mutex> lock(m_receive_lock);
        LOG_DEBUG_STR("Websocket: Process message release");

        unique_lock<mutex> con_lock(m_connection_lock);
        m_connections.insert(a.hdl);
        m_luahandles[++luaHandleMax] = a.hdl;
        m_luahandles_rev[a.hdl] = luaHandleMax;
      } else if (a.type == UNSUBSCRIBE) {
        LOG_DEBUG_STR("Websocket: Process message lock 2");
        unique_lock<mutex> lock(m_receive_lock);
        LOG_DEBUG_STR("Websocket: Process message release 2");
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
    } catch (std::exception& ex) {
      LOG_ERR_MSG("Exception in websocket run loop", ex.what());
    } catch (...) {
      LOG_ERR_STR("Caught (...) in websocket run loop");
    }
    LOG_DEBUG_STR("Websocket: Process message end");
  }
}

void WebsocketServer::broadcast(const std::string& s) {
  LOG_DEBUG_STR("Websocket: broadcast");
  unique_lock<mutex> lock(m_action_lock);
  m_actions.push(action(MESSAGE, s));
  lock.unlock();
  m_action_cond.notify_one();
  LOG_DEBUG_STR("Websocket: broadcast end");
}

std::pair<size_t, std::string> WebsocketServer::receive() {
  LOG_DEBUG_STR("Websocket: Receive");
  unique_lock<mutex> lock(m_receive_lock);
  LOG_DEBUG_STR("Websocket: Receive lock");
  lua_message msg;
  if(!m_receive.empty()) {
    msg = m_receive.front();
    m_receive.pop_front();
  } else {
    m_receive_cond.wait(lock);
    msg = m_receive.front();
    m_receive.pop_front();
  }
  LOG_DEBUG_STR("Websocket: Receive end");
  return msg;
}

void WebsocketServer::send(size_t handle, const std::string& message) {
  LOG_DEBUG_STR("Websocket: Send");
  unique_lock<mutex> con_lock(m_connection_lock);
  unique_lock<mutex> lock(m_receive_lock);
  LOG_DEBUG_STR("Websocket: Send release");

  try {
    auto it = m_luahandles.find(handle);
    if(it != m_luahandles.end())
      m_server.send((*it).second, message, websocketpp::frame::opcode::TEXT);
  } catch (std::exception& ex) {
    LOG_ERR_MSG("Exception in websocket run loop", ex.what());
  } catch (...) {
    LOG_ERR_STR("Caught (...) in websocket run loop");
  }
  LOG_DEBUG_STR("Websocket: Send end");
}

void WebsocketServer::init(int port) {
  assert(server_instance == NULL);
  server_instance = new WebsocketServer();
  std::thread maint([=](){
    server_instance->run(port);
  });
  thread t(bind(&WebsocketServer::process_messages, server_instance));

  t.detach();
  maint.detach();
}

WebsocketServer* WebsocketServer::getInstance() {
  assert(server_instance != NULL);
  return server_instance;
}

WebsocketServer* WebsocketServer::server_instance = NULL;
}
}

