#include "websocket.hpp"
#include "websocketpp/endpoint.hpp"
#include "exithandler.hpp"
#include "logger.hpp"
#include "exception.hpp"

#include <fstream>
#include <cryptopp/base64.h>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>
#include <cryptopp/osrng.h>
#include <cryptopp/pwdbased.h>
#include <random>

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

std::string make_sessionkey() {
  std::mt19937 rng;
  rng.seed(std::random_device()());
  std::uniform_real_distribution<float> dist(0.0,1.0);

  string strRng = std::to_string(dist(rng));
  CryptoPP::SHA256 hash;
  byte digest[CryptoPP::SHA256::DIGESTSIZE];
  std::string output;

  hash.CalculateDigest(digest,(const byte *)strRng.c_str(),strRng.size());

  CryptoPP::HexEncoder encoder;
  CryptoPP::StringSink *SS = new CryptoPP::StringSink(output);
  encoder.Attach(SS);
  encoder.Put(digest,sizeof(digest));
  encoder.MessageEnd();

  return output;
}

std::pair<string,string> hash_password(const string password, string salthex = "") {
   using namespace CryptoPP;
   unsigned int iterations = 1000000;

   SecByteBlock derivedkey(AES::DEFAULT_KEYLENGTH);

   if(salthex.empty()) {
     SecByteBlock pwsalt(AES::DEFAULT_KEYLENGTH);
     AutoSeededRandomPool rng;

     rng.GenerateBlock(pwsalt,pwsalt.size());
     StringSource ss1(pwsalt,pwsalt.size(),true,
            new HexEncoder(
               new StringSink(salthex)
            )
     );

     PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> p;
     p.DeriveKey(
        derivedkey, derivedkey.size(),
        0x00,
        (byte *) password.data(), password.size(),
        (byte *) salthex.data(), salthex.size(),
        iterations
     );
   } else {
     PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> p;
     p.DeriveKey(
        derivedkey, derivedkey.size(),
        0x00,
        (byte *) password.data(), password.size(),
        (byte *) salthex.data(), salthex.size(),
        iterations
     );
   }

   std::string derivedhex;
   StringSource ss2(derivedkey,derivedkey.size(),true,
          new HexEncoder(
             new StringSink(derivedhex)
          )
        );
   return {derivedhex, salthex};
}

WebsocketServer::WebsocketServer(const std::string passwdFile) : passwdFile(passwdFile) {
  if(passwdFile.empty()) {
    this->doAuthenticate = false;
  } else {
    this->doAuthenticate = true;
    readAuthData(passwdFile);
  }
  LOG_DEBUG_STR("Websocket: Init");

  m_server.set_access_channels(websocketpp::log::alevel::none);
  m_server.set_error_channels(websocketpp::log::elevel::all);
  // Initialize Asio Transport
  m_server.init_asio();

  // Register handler callbacks
  m_server.set_open_handler(bind(&WebsocketServer::on_open, this, std::placeholders::_1));
  m_server.set_close_handler(bind(&WebsocketServer::on_close, this, std::placeholders::_1));
  m_server.set_message_handler(bind(&WebsocketServer::on_message, this, std::placeholders::_1, std::placeholders::_2));

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

string WebsocketServer::loginUser(const connection_hdl h, const std::string& sessionKey) {
  if(sessionMap.find(sessionKey) == sessionMap.end()) {
    return "session-invalid";
  } else {
    unique_lock<mutex> lock(m_receive_lock);
    ConnectionHandle old = sessionMap[sessionKey];
    if(std::owner_less<ConnectionHandle>()(old,h.lock()) || std::owner_less<ConnectionHandle>()(h.lock(),old)) {
      string username = authMap[old];
      authMap[h.lock()] = username;
      authMap.erase(old);
      size_t lh = m_luahandles_rev[old];
      m_luahandles_rev[h.lock()] = lh;
      m_luahandles_rev.erase(old);
      m_luahandles[lh] = h.lock();
      sessionMap[sessionKey] = h.lock();
    }
    return "session-success:" + sessionKey;
  }
}

string WebsocketServer::loginUser(const connection_hdl h, const std::string& username, const std::string& password) {
  if(username.find(' ') < username.size() || password.find(' ') < password.size())
    return "login-invalid";

  if(authData.find(username) == authData.end()) {
    return "login-notfound";
  } else {
    string sessionKey = make_sessionkey();
    Credentials& c = authData[username];
    std::pair<string,string> hashed = hash_password(password, c.salt);
    if(c.hash == hashed.first) {
      sessionMap[sessionKey] = h.lock();
      std::cerr << "insert: " << *static_cast<int*>(sessionMap[sessionKey].get()) << std::endl;
      authMap[h.lock()] = username;
      return "login-success:" + sessionKey;
    } else {
      return "login-nomatch";
    }
  }
}

string WebsocketServer::registerUser(const connection_hdl h, const std::string& username, const std::string& password, const std::string& userdata) {
  if(username.find(' ') < username.size() || password.find(' ') < password.size())
    return "register-invalid";

  if(authData.find(username) == authData.end()) {
    std::pair<string,string> hashed = hash_password(password);
    string sessionKey = make_sessionkey();
    authData[username] = {hashed.first, hashed.second, userdata};
    authMap[h.lock()] = username;
    sessionMap[sessionKey] = h.lock();
    std::cerr << "insert: " << *static_cast<int*>(sessionMap[sessionKey].get()) << std::endl;
    std::ofstream outfile;
    outfile.open(this->passwdFile, std::ios_base::app);
    string userData64;
    using namespace CryptoPP;
    StringSource ss1(userdata,true, new Base64Encoder(new StringSink(userData64)));
    outfile << username << " " << hashed.first << " " << hashed.second << " " << userData64;
    return "register-success:" + sessionKey;
  } else {
    return "register-duplicate";
  }
}

void WebsocketServer::readAuthData(const std::string& passwdFile) {
  std::ifstream ifs(passwdFile);
  std::string line;
  std::string token;
  std::vector<std::string> tokens;
  std::stringstream ss;
  while(std::getline(ifs, line)) {
    ss.str(line);
    while(std::getline(ss, token, ' ')) {
      tokens.push_back(token);
    }
    if(tokens.size() != 4)
      throw janosh_exception() << string_info({"Corrupt passwd file", passwdFile});

    string userdata;
    using namespace CryptoPP;
    StringSource ss1(tokens[3], true, new Base64Decoder(new StringSink(userdata)));
    authData[tokens[0]] = {tokens[1], tokens[2], userdata};
  }
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

void WebsocketServer::on_message(connection_hdl h, server::message_ptr msg) {
  if((doAuthenticate && authMap.find(h.lock()) != authMap.end()) || !doAuthenticate) {
    LOG_DEBUG_STR("Websocket: On message");
    unique_lock<mutex> lock(m_receive_lock);
    LOG_DEBUG_STR("Websocket: On message lock");

    if(m_luahandles_rev.find(h.lock()) != m_luahandles_rev.end())
      m_receive.push_back(std::make_pair(m_luahandles_rev[h.lock()], msg->get_payload()));

    lock.unlock();
    m_receive_cond.notify_one();
  } else {
    const std::string& payload = msg->get_payload();
    std::stringstream ss(payload);
    std::vector<string> tokens;
    string token;
    while(std::getline(ss, token)) {
        tokens.push_back(token);
    }

    if(tokens.size() == 4 && tokens[0] == "register") {
      string response = registerUser(h, tokens[1], tokens[2], tokens[3]);
      this->send(m_luahandles_rev[h.lock()], response);
    } else if(tokens.size() == 3 && tokens[0] == "login") {
      string response = loginUser(h, tokens[1], tokens[2]);
      this->send(m_luahandles_rev[h.lock()], response);
    } else if(tokens.size() == 2 && tokens[0] == "login") {
      string response = loginUser(h, tokens[1]);
      this->send(m_luahandles_rev[h.lock()], response);
    } else
      this->send(m_luahandles_rev[h.lock()], "auth");
  }


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
        m_connections.insert(a.hdl.lock());
        m_luahandles[++luaHandleMax] = a.hdl.lock();
        m_luahandles_rev[a.hdl.lock()] = luaHandleMax;
      } else if (a.type == UNSUBSCRIBE) {
        LOG_DEBUG_STR("Websocket: Process message lock 2");
        unique_lock<mutex> lock(m_receive_lock);
        LOG_DEBUG_STR("Websocket: Process message release 2");
        unique_lock<mutex> con_lock(m_connection_lock);
        m_connections.erase(a.hdl.lock());
        size_t intHandle = m_luahandles_rev[a.hdl.lock()];
        m_luahandles.erase(intHandle);
        m_luahandles_rev.erase(a.hdl.lock());
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

string WebsocketServer::getUserData(size_t luahandle) {
  auto it = m_luahandles.find(luahandle);
   if(it != m_luahandles.end() && authMap.find((*it).second) != authMap.end()) {
     return authData[authMap[(*it).second]].userData;
   }
   return "";
}

string WebsocketServer::getUserName(size_t luahandle) {
  auto it = m_luahandles.find(luahandle);
  if (it == m_luahandles.end()) {
    std::cerr << "#################luahandle not found" << std::endl;
  } else {
    if (authMap.find((*it).second) == authMap.end())
      std::cerr << "##################hdl not found" << std::endl;
  }
  if (it != m_luahandles.end() && authMap.find((*it).second) != authMap.end()) {
    std::cerr << "##################found" << std::endl;

    return authMap[(*it).second];
  }
  return "invalid";
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

void WebsocketServer::send(size_t luahandle, const std::string& message) {
  LOG_DEBUG_STR("Websocket: Send");
  unique_lock<mutex> con_lock(m_connection_lock);
  unique_lock<mutex> lock(m_receive_lock);
  LOG_DEBUG_STR("Websocket: Send release");

  try {
    auto it = m_luahandles.find(luahandle);
    if(it != m_luahandles.end())
      m_server.send((*it).second, message, websocketpp::frame::opcode::TEXT);
  } catch (std::exception& ex) {
    LOG_ERR_MSG("Exception in websocket run loop", ex.what());
  } catch (...) {
    LOG_ERR_STR("Caught (...) in websocket run loop");
  }
  LOG_DEBUG_STR("Websocket: Send end");
}

void WebsocketServer::init(const int port, const string passwdFile) {
  assert(server_instance == NULL);
  server_instance = new WebsocketServer(passwdFile);
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

