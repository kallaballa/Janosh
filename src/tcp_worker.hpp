#ifndef TCP_WORKER_HPP_
#define TCP_WORKER_HPP_

#include "janosh_thread.hpp"
#include "shared_pointers.hpp"
#include "request.hpp"
#include "semaphore.hpp"
#include "janosh.hpp"

namespace janosh {

class MessageQueue {
public:
  struct Item {
    string otherid;
    string id;
    string request;
  };
private:
  std::mutex m_;
  std::queue<Item> q_;
public:
  void push(string otherid, string id, string request) {
    std::unique_lock<std::mutex> lock(m_);
    q_.push({otherid, id, request});
  }

  Item pop() {
    std::unique_lock<std::mutex> lock(m_);
    if(!q_.empty()) {
    Item i = q_.front();
    q_.pop();
    return i;
    } else {
      return {"","",""};
    }
  }
};

class TcpWorker : public JanoshThread {
  shared_ptr<Janosh> janosh_;
  shared_ptr<Semaphore> threadSema_;
  zmq::context_t* context_;
  zmq::socket_t socket_;
  Request readRequest();
  MessageQueue msgQueue_;
  static std::mutex tidMutex_;
  static std::string ctid_;


public:
  explicit TcpWorker(int maxThreads, zmq::context_t* context);
  static void setCurrentTransactionID(const string& tid);
  static string getCurrentTransactionID();

  void process(string otherID, string id, string request, bool transaction);
  void run();
  bool connected();
};


} /* namespace janosh */

#endif /* TCP_WORKER_HPP_ */
