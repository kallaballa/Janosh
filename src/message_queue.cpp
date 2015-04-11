/*
 * tracker.cpp
 *
 *  Created on: Feb 15, 2014
 *      Author: elchaschab
 */

#include "message_queue.hpp"
#include "logger.hpp"
#include <sstream>

namespace janosh {
using std::cerr;
using std::endl;

MessageQueue* MessageQueue::instance_ = NULL;

MessageQueue::MessageQueue() :
    context_(1), publisher_(context_, ZMQ_PUB) {
  const char * val = std::getenv("USER");

  if (val == NULL) {
    LOG_ERR_STR("Environment variable USER not found");
  } else {
    string url = (string("ipc:///tmp/janosh-") + string(val) + string(".ipc"));
    LOG_INFO_MSG("Binding ZMQ", url);
    publisher_.bind(url.c_str());
  }
}

MessageQueue::~MessageQueue() {
  publisher_.close();
  context_.close();
}

void MessageQueue::publish(const string& key, const string& op, const char* value) {
  std::stringstream ss;
  ss << key << ' ' << op << ' ' << value;
  const string& str = ss.str();
  zmq::message_t message(str.length());
  memcpy(message.data(), str.data(),str.length());
  publisher_.send(message);
}
} /* namespace janosh */
