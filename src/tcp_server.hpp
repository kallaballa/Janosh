/*
 * CheesyServer.h
 *
 *  Created on: Jan 6, 2014
 *      Author: elchaschab
 */

#ifndef TCPSERVER_H_
#define TCPSERVER_H_

#include <boost/asio.hpp>
#include "format.hpp"
#include <mutex>
#include <condition_variable>

namespace janosh {

class Semaphore
{
private:
    std::mutex mutex_;
    std::condition_variable condition_;
    unsigned long count_; // Initialized as locked.

public:
    Semaphore(unsigned long count) : count_(count) {}
    void notify() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        ++count_;
        condition_.notify_one();
    }

    void wait() {
        std::unique_lock<decltype(mutex_)> lock(mutex_);
        while(!count_) // Handle spurious wake-ups.
            condition_.wait(lock);
        --count_;
    }

    bool try_wait() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);
        if(count_) {
            --count_;
            return true;
        }
        return false;
    }
};

using boost::asio::ip::tcp;
class TcpServer {
  boost::asio::io_service io_service_;
  boost::asio::ip::tcp::acceptor acceptor_;
  static TcpServer* instance_;
  Semaphore* threadSema_;
  TcpServer(int maxThreads);

public:
	virtual ~TcpServer();
	bool isOpen();
  void open(int port);
	void close();
	bool run();

	static TcpServer* getInstance(int maxThreads) {
	  if(instance_ == NULL)
	    instance_ = new TcpServer(maxThreads);
	  return instance_;
	}
};

} /* namespace janosh */

#endif /* CHEESYSERVER_H_ */
