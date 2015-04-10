/*
 * tracker.hpp
 *
 *  Created on: Feb 15, 2014
 *      Author: elchaschab
 */

#ifndef MESSAGE_QUEUE_HPP_
#define MESSAGE_QUEUE_HPP_

#include <string>
#include "exception.hpp"
#include <iostream>
#include <thread>
#include "cppzmq/zmq.hpp"

namespace janosh {
using std::string;
using std::ostream;
using std::thread;
class MessageQueue {
public:
  void publish(const string& key, const string& op, const char* value);
  static MessageQueue* getInstance() {
    if(instance_ == NULL) {
      instance_ = new MessageQueue();
    }
    return instance_;
  }
private:
  MessageQueue();
  ~MessageQueue();

  static MessageQueue* instance_;
  zmq::context_t context_;
  zmq::socket_t publisher_;
};

} /* namespace janosh */

#endif /* MESSAGE_QUEUE_HPP_ */
