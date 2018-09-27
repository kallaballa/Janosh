#include "websocket.hpp"
#include "exithandler.hpp"
#include "logger.hpp"
#include "exception.hpp"
#include "semaphore.hpp"

#include <fstream>
#include <cryptopp/base64.h>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>
#include <cryptopp/osrng.h>
#include <cryptopp/pwdbased.h>
#include <random>
#include <algorithm>
#include <shared_pointers.hpp>
#include <mutex>
#include <functional>
#include <thread>


namespace janosh {
namespace lua {
using std::unique_lock;


std::string make_sessionkey() {
  std::mt19937 rng;
  rng.seed(std::random_device()());
  std::uniform_real_distribution<double> dist(0.0,1.0);

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

     CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> p;
     p.DeriveKey(
        derivedkey, derivedkey.size(),
        0x00,
        (byte *) password.data(), password.size(),
        (byte *) salthex.data(), salthex.size(),
        iterations
     );
   } else {
     CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> p;
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

bool WebsocketServer::Authenticator::hasUsername(const std::string& username) {
  return authData.find(username) != authData.end();
}

bool WebsocketServer::Authenticator::hasSession(const std::string& sessionKey) {
  return skeyConMap.find(sessionKey) != skeyConMap.end();
}

bool WebsocketServer::Authenticator::hasConnectionHandle(connection_hdl c) {
  return conSkeyMap.find(c) != conSkeyMap.end();
}

bool WebsocketServer::Authenticator::hasLuaHandle(size_t l) {
  return m_luahandles.find(l) != m_luahandles.end();
}

void WebsocketServer::Authenticator::remapSession(const connection_hdl h, const std::string& sessionKey) {
  connection_hdl old = skeyConMap[sessionKey];
  if(old != h) {
    if(m_luahandles_rev.find(old) != m_luahandles_rev.end()) {
      size_t lh = m_luahandles_rev[old];
      m_luahandles_rev[h] = lh;
      m_luahandles_rev.erase(old);
      m_luahandles[lh] = h;
      skeyConMap[sessionKey] = h;
      conSkeyMap[h] = sessionKey;
    } else {
      assert(m_luahandles_rev.find(h) != m_luahandles_rev.end());
      skeyConMap[sessionKey] = h;
      conSkeyMap[h] = sessionKey;
    }
  }
}


string WebsocketServer::Authenticator::createSession(const connection_hdl h, const std::string& username, const std::string& password) {
  string sessionKey = make_sessionkey();
  skeyNameMap[sessionKey] = username;
  nameSkeyMap.insert({username, sessionKey});
  Credentials& c = authData[username];
  std::pair<string,string> hashed = hash_password(password, c.salt);
  if(c.hash == hashed.first) {
    skeyConMap[sessionKey] = h;
    conSkeyMap[h] = sessionKey;
    return sessionKey;
  } else {
    return "";
  }
}

void WebsocketServer::Authenticator::destroySession(const std::string& sessionKey) {
  assert(skeyNameMap.find(sessionKey) != skeyNameMap.end());
  string username = skeyNameMap[sessionKey];

  auto itpair = nameSkeyMap.equal_range(username);
  for (auto it = itpair.first; it != itpair.second; ++it) {
      if (it->second == sessionKey) {
          nameSkeyMap.erase(it);
          break;
      }
  }
  auto itsc =skeyConMap.find(sessionKey);
  assert(itsc != skeyConMap.end());
  connection_hdl c = (*itsc).second;

  auto itcs = conSkeyMap.find(c);
  assert(itcs != conSkeyMap.end());
  skeyConMap.erase(itsc);
  conSkeyMap.erase(itcs);
}

string WebsocketServer::Authenticator::createUser(const connection_hdl h, const std::string& username, const std::string& password,
    const std::string& userdata) {
  std::pair<string, string> hashed = hash_password(password);
  string sessionKey = make_sessionkey();
  skeyNameMap[sessionKey] = username;
  nameSkeyMap.insert( { username, sessionKey });

  authData[username] = {hashed.first, hashed.second, userdata};
  skeyConMap[sessionKey] = h;
  conSkeyMap[h] = sessionKey;
  std::ofstream outfile;
  outfile.open(this->passwdFile, std::ios_base::app);
  string userData64;
  using namespace CryptoPP;
  StringSource ss1(userdata, true, new Base64Encoder(new StringSink(userData64)));
  userData64.erase(std::remove(userData64.begin(), userData64.end(), '\n'), userData64.end());
  outfile << username << " " << hashed.first << " " << hashed.second << " " << userData64 << std::endl;
  return sessionKey;
}

void WebsocketServer::Authenticator::readAuthData(const std::string& passwdFile) {
  std::ifstream ifs(passwdFile);
  std::string line;
  std::string token;
  std::vector<std::string> tokens;

  while(std::getline(ifs, line)) {
    std::istringstream ss(line);
    while(std::getline(ss, token, ' ')) {
      tokens.push_back(token);
    }
    if(tokens.size() != 4)
      throw janosh_exception() << string_info({"Corrupt passwd file", passwdFile});

    string userdata;
    using namespace CryptoPP;
    StringSource ss1(tokens[3], true, new Base64Decoder(new StringSink(userdata)));
    if(authData.find(tokens[0]) != authData.end())
      throw janosh_exception() << string_info({"Corrupt passwd file", passwdFile});

    authData[tokens[0]] = {tokens[1], tokens[2], userdata};
    tokens.clear();
  }
}

string WebsocketServer::Authenticator::getUserData(size_t luahandle) {
  auto it = m_luahandles.find(luahandle);
  if (it != m_luahandles.end() && conSkeyMap.find((*it).second) != conSkeyMap.end()) {
    return authData[skeyNameMap[conSkeyMap[(*it).second]]].userData;
  }
  return "";
}

string WebsocketServer::Authenticator::getUserName(size_t luahandle) {
  auto it = m_luahandles.find(luahandle);
  if (it != m_luahandles.end() && conSkeyMap.find((*it).second) != conSkeyMap.end()) {
    return skeyNameMap[conSkeyMap[(*it).second]];
  }
  return "";
}

std::vector<size_t> WebsocketServer::Authenticator::getHandles(const string& username) {
  std::vector<size_t> handles;
  const auto& range = nameSkeyMap.equal_range(username);
  for(auto it = range.first; it != range.second; ++it) {
    const auto& itsc = skeyConMap.find((*it).second);
    assert(itsc != skeyConMap.end());
    const auto& itlhr = m_luahandles_rev.find((*itsc).second);
    assert(itlhr != m_luahandles_rev.end());
    handles.push_back((*itlhr).second);
  }

  return handles;
}

size_t WebsocketServer::Authenticator::getLuaHandle(connection_hdl c) {
  if(m_luahandles_rev.find(c) == m_luahandles_rev.end()) {
    throw janosh_exception() << msg_info("Lua handle doesn't exist anymore");
  }
  return m_luahandles_rev[c];
}

connection_hdl WebsocketServer::Authenticator::getConnectionHandle(size_t luaHandle) {
  if(m_luahandles.find(luaHandle) == m_luahandles.end()) {
    throw janosh_exception() << msg_info("Lua handle doesn't exist anymore: " + std::to_string(luaHandle));
  }
  return m_luahandles[luaHandle];
}
size_t WebsocketServer::Authenticator::createLuaHandle(connection_hdl c) {
  size_t handle = ++luaHandleMax;
  m_luahandles[handle] = c;
  m_luahandles_rev[c] = handle;
  return handle;
}

void WebsocketServer::Authenticator::destroyLuaHandle(connection_hdl c) {
  size_t handle = m_luahandles_rev[c];
  m_luahandles.erase(handle);
  m_luahandles_rev.erase(c);
  if(conSkeyMap.find(c) != conSkeyMap.end())
    destroySession(conSkeyMap[c]);
}

WebsocketServer::WebsocketServer(const std::string passwdFile) : m_server(), messageSema(10) {
  if(passwdFile.empty()) {
    this->doAuthenticate = false;
  } else {
    this->doAuthenticate = true;
    auth.readAuthData(passwdFile);
  }
  m_server.onConnection([=](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
      this->on_open(ws);
  });

  m_server.onDisconnection([=](uWS::WebSocket<SERVER> *ws, int code, char *message, size_t length) {
      this->on_close(ws);
  });
  m_server.onMessage([=](WebSocket<SERVER> *ws, char *message, size_t length, OpCode opCode){
    this->on_message(ws, message, length, opCode);
  });

  ExitHandler::getInstance()->addExitFunc([&](){
    LOG_DEBUG_STR("Shutdown");

    LOG_DEBUG_STR("Stopped");
  });
  LOG_DEBUG_STR("Websocket: Init end");
}

WebsocketServer::~WebsocketServer() {
}

void WebsocketServer::run(uint16_t port) {
  LOG_DEBUG_STR("Websocket: Listen");

  // listen on specified port
  if (m_server.listen(port, nullptr, uS::ListenOptions::REUSE_PORT)) {
    try {
      LOG_DEBUG_STR("Websocket: Run");
      m_server.run();
      LOG_DEBUG_STR("Websocket: End");
    } catch (const std::exception & e) {
      throw e;
    }
    LOG_DEBUG_STR("Websocket: End");
  } else {
    throw janosh_exception() << msg_info("Websocket can't listen");
  }
}

string WebsocketServer::loginUser(const connection_hdl h, const std::string& sessionKey) {
  if(!auth.hasSession(sessionKey)) {
    return "session-invalid";
  } else {
    unique_lock<mutex> lock(m_receive_lock);
    unique_lock<mutex> lock1(m_action_lock);
    unique_lock<mutex> lock2(m_connection_lock);
    auth.remapSession(h,sessionKey);
    return "session-success:" + sessionKey;
  }
}

string WebsocketServer::loginUser(const connection_hdl h, const std::string& username, const std::string& password) {
  if(username.find(' ') < username.size() || password.find(' ') < password.size())
    return "login-invalid";

  if(!auth.hasUsername(username)) {
    return "login-notfound";
  } else {
    string sessionKey = auth.createSession(h, username, password);
    if(!sessionKey.empty()) {
      return "login-success:" + sessionKey;
    } else {
      return "login-nomatch";
    }
  }
}

string WebsocketServer::registerUser(const connection_hdl h, const std::string& username, const std::string& password, const std::string& userdata) {
  if(username.find(' ') < username.size() || password.find(' ') < password.size())
    return "register-invalid";

  if(!auth.hasUsername(username)) {
    string sessionKey = auth.createUser(h, username, password, userdata);
    if(!sessionKey.empty())
      return "register-success:" + sessionKey;
    else
      return "register-failed";
  } else {
    return "register-duplicate";
  }
}
bool WebsocketServer::logoutUser(const string& sessionKey) {
  if(auth.hasSession(sessionKey)) {
    auth.destroySession(sessionKey);
    return true;
  } else {
    return false;
  }
}

void WebsocketServer::on_open(uWS::WebSocket<uWS::SERVER> *ws) {
  LOG_DEBUG_STR("Websocket: Open");
  unique_lock<mutex> lock(m_action_lock);
  m_actions.push(action(SUBSCRIBE, connection_hdl(ws)));
  lock.unlock();
  m_action_cond.notify_one();
  LOG_DEBUG_STR("Websocket: Open end");
}

void WebsocketServer::on_close(uWS::WebSocket<uWS::SERVER> *ws) {
  LOG_DEBUG_STR("Websocket: Close");
  unique_lock<mutex> lock(m_action_lock);
  m_actions.push(action(UNSUBSCRIBE, connection_hdl(ws)));
  lock.unlock();
  m_action_cond.notify_one();
  LOG_DEBUG_STR("Websocket: Close end");
}

void WebsocketServer::on_message(WebSocket<SERVER> *ws, char *message, size_t length, OpCode opCode) {
  std::string payload;
  for(size_t i = 0; i < length; ++i) {
    payload.push_back(message[i]);
  }
  if((doAuthenticate && auth.hasConnectionHandle(ws)) || !doAuthenticate) {
    std::stringstream ss(payload);
    std::vector<string> tokens;
    string token;
    while(std::getline(ss, token)) {
        tokens.push_back(token);
    }
    if(tokens.size() == 2 && tokens[0] == "logout" && auth.hasSession(tokens[1])) {
      bool success = logoutUser(tokens[1]);
      string response;
      if(success)
        response = "logout-success";
      else
        response = "logout-nosession";

      this->send(auth.getLuaHandle(ws), response);
    } else {
//      if(messageSema.would_wait()) {
//        LOG_DEBUG_STR("Websocket: Backlog detected");
//      }

      messageSema.wait();
      LOG_DEBUG_STR("Websocket: On message");
      unique_lock<mutex> lock(m_receive_lock);
      LOG_DEBUG_STR("Websocket: On message lock");

      m_receive.push_back(std::make_pair(auth.getLuaHandle(ws), payload));
      lock.unlock();
      m_receive_cond.notify_one();
    }
  } else {
    std::stringstream ss(payload);
    std::vector<string> tokens;
    string token;
    while(std::getline(ss, token)) {
        tokens.push_back(token);
    }

    if(tokens.size() == 4 && tokens[0] == "register") {
      string response = registerUser(ws, tokens[1], tokens[2], tokens[3]);
      this->send(auth.getLuaHandle(ws), response);
    } else if(tokens.size() == 3 && tokens[0] == "login") {
      string response = loginUser(ws, tokens[1], tokens[2]);
      this->send(auth.getLuaHandle(ws), response);
    } else if(tokens.size() == 2 && tokens[0] == "login") {
      string response = loginUser(ws, tokens[1]);
      this->send(auth.getLuaHandle(ws), response);
    } else
      this->send(auth.getLuaHandle(ws), "auth");
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
        m_connections.insert(a.hdl);
        auth.createLuaHandle(a.hdl);
      } else if (a.type == UNSUBSCRIBE) {
        LOG_DEBUG_STR("Websocket: Process message lock 2");
        unique_lock<mutex> lock(m_receive_lock);
        LOG_DEBUG_STR("Websocket: Process message release 2");
        unique_lock<mutex> con_lock(m_connection_lock);

        m_connections.erase(a.hdl);
        auth.destroyLuaHandle(a.hdl);
      } else if (a.type == MESSAGE) {
        unique_lock<mutex> con_lock(m_connection_lock);

        con_list::iterator it;
        a.hdl->send(a.msg.c_str(), a.msg.size(), OpCode::TEXT);

      } else if(a.type == BROADCAST) {
        unique_lock<mutex> con_lock(m_connection_lock);
        m_server.getDefaultGroup<uWS::SERVER>().broadcast(a.msg.c_str(), a.msg.size(), OpCode::TEXT);
      } else {
        assert(false);
      }
    } catch (janosh_exception& ex) {
      LOG_ERR_MSG("Exception in websocket run loop", ex.what());
      printException(ex);
    } catch (std::exception& ex) {
      LOG_ERR_MSG("Exception in websocket run loop", ex.what());
    } catch (...) {
      LOG_ERR_STR("Caught (...) in websocket run loop");
    }
    LOG_DEBUG_STR("Websocket: Process message end");
  }
}

string WebsocketServer::getUserData(size_t luahandle) {
  string userdata = auth.getUserData(luahandle);
  if(userdata.empty())
    return "invalid";
  else
    return userdata;
}

string WebsocketServer::getUserName(size_t luahandle) {
  string username = auth.getUserName(luahandle);
  if(username.empty())
    return "invalid";
  else
    return username;
}

std::vector<size_t> WebsocketServer::getHandles(const string& username) {
  return auth.getHandles(username);
}

void WebsocketServer::broadcast(const std::string& s) {
  LOG_DEBUG_STR("Websocket: broadcast");
  unique_lock<mutex> lock(m_action_lock);
  m_actions.push(action(BROADCAST, s));
  lock.unlock();
  m_action_cond.notify_one();
  LOG_DEBUG_STR("Websocket: broadcast end");
}

std::pair<size_t, std::string> WebsocketServer::receive() {
  LOG_DEBUG_STR("Websocket: Receive");
  unique_lock<mutex> lock(m_receive_lock);
  LOG_DEBUG_STR("Websocket: Receive lock");
  lua_message msg;

  if (!m_receive.empty()) {
    msg = m_receive.front();
    m_receive.pop_front();
    messageSema.notify();
  } else {
    while(m_receive.empty()) {
      LOG_DEBUG_STR("Websocket: Receive wait");
      m_receive_cond.wait(lock);
      LOG_DEBUG_STR("Websocket: Receive wait end");
      if (!m_receive.empty()) {
        msg = m_receive.front();
        m_receive.pop_front();
        messageSema.notify();
        break;
      }
    }
  }

  LOG_DEBUG_STR("Websocket: Receive end");
  return msg;
}

void WebsocketServer::send(size_t luahandle, const std::string& message) {
  LOG_DEBUG_STR("Websocket: Send");
  unique_lock<mutex> con_lock(m_connection_lock);
  LOG_DEBUG_STR("Websocket: Send release");
  try {
    connection_hdl c = auth.getConnectionHandle(luahandle);
    LOG_DEBUG_MSG("Websocket: Payload", message);

    c->send(message.c_str(), message.size(),OpCode::TEXT);
  } catch (janosh_exception& ex) {
    printException(ex);
  }
  LOG_DEBUG_STR("Websocket: Send end");
}

void WebsocketServer::init(const int port, const string passwdFile) {
  assert(server_instance == NULL);
  server_instance = new WebsocketServer(passwdFile);
  std::thread maint([=](){
    janosh::Logger::getInstance().registerThread("Websocket-RunLoop");
    server_instance->run(port);
  });
  std::thread t([=]() {
    janosh::Logger::getInstance().registerThread("Websocket-ProcessMessages");
    server_instance->process_messages();
  });

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

