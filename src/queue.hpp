/*
 * queue.hpp
 *
 *  Created on: Sep 30, 2018
 *      Author: elchaschab
 */

#ifndef SRC_QUEUE_HPP_
#define SRC_QUEUE_HPP_

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class Queue
{
 public:

  T pop()
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    while (queue_.empty())
    {
      cond_.wait(mlock);
    }
    auto item = queue_.front();
    queue_.pop();
    return item;
  }

  void pop(T& item)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    while (queue_.empty())
    {
      cond_.wait(mlock);
    }
    item = queue_.front();
    queue_.pop();
  }

  void push(const T& item)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    queue_.push(item);
    mlock.unlock();
    cond_.notify_one();
  }

  void push(T&& item)
  {
    std::unique_lock<std::mutex> mlock(mutex_);
    queue_.push(std::move(item));
    mlock.unlock();
    cond_.notify_one();
  }

  size_t size() {
    std::unique_lock<std::mutex> mlock(mutex_);
    return queue_.size();
  }

  size_t empty() {
    std::unique_lock<std::mutex> mlock(mutex_);
    return queue_.empty();
  }

 private:
  std::queue<T> queue_;
  std::mutex mutex_;
  std::condition_variable cond_;
};



#endif /* SRC_QUEUE_HPP_ */
