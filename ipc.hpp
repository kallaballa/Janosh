#ifndef IPC_HPP_
#define IPC_HPP_

#include <cstring>
#include <functional>
#include <string>
#include <algorithm>
#include <boost/exception/all.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/thread.hpp>

using boost::interprocess::shared_memory_object;
using boost::interprocess::interprocess_semaphore;
using boost::interprocess::mapped_region;
using boost::interprocess::create_only_t;
using boost::interprocess::open_only_t;
using boost::this_thread::sleep;
using boost::posix_time::milliseconds;

template <std::streamsize T_size, typename T_char_type >
class basic_ringbuffer_connection {

private:
  typedef boost::interprocess::string t_string;

  typedef uint8_t t_sync_state;
  typedef boost::interprocess::interprocess_semaphore t_ipc_semaphore;
  typedef boost::interprocess::message_queue t_msg_queue;

  shared_memory_object shm_;
  mapped_region region_;

  const std::string name_;

  volatile t_sync_state* state_;
  t_ipc_semaphore*  mutex_;

  //the underlying ringbuffer manages memory and synchronisation itself
  t_msg_queue* mq_;
  bool daemon_;
  static const t_sync_state FIN_ = 0;
  static const t_sync_state ACK_ = 1;
  static const t_sync_state SIN_ = 2;
  static const t_sync_state CLS_ = 3;
public:
  bool shm_init_;
  struct connection_error: virtual boost::exception { };

  typedef std::char_traits<T_char_type> t_char_traits;
  typedef basic_ringbuffer_connection<T_size, T_char_type> type;

  struct Sink {
    typedef T_char_type  char_type;
    typedef boost::iostreams::sink_tag  category;

    type* t;
    Sink(type* t): t(t)
    {}

    std::streamsize write(const T_char_type* s, std::streamsize n) {
      for(int i = 0; i < n; ++i) {
        t->mq_->send(s, sizeof(T_char_type), 0);
        ++s;
      }
      return n;
    }
  };

  struct Source {
      typedef T_char_type char_type;
      typedef boost::iostreams::source_tag  category;
      type* t;
      Source(type* t): t(t)
      {}

      std::streamsize read(char* s, std::streamsize n) {
        size_t rs;
        unsigned int priority;
        type::t_msg_queue::size_type num = std::min(t->mq_->get_num_msg(), boost::lexical_cast<type::t_msg_queue::size_type>(n));
        if(t->isOpen() && num == 0)
          ++num;
        for(size_t i = 0; i < num; ++i) {
          t->mq_->receive(s, sizeof(T_char_type), rs, priority);
          if(rs == 0)
            return i;
          ++s;
        }
        return num;
      }
  };

  typedef boost::iostreams::stream<Sink> ostream;
  typedef boost::iostreams::stream<Source> istream;

  basic_ringbuffer_connection(const char* name) :
      name_(name),
      mutex_(NULL),
      shm_init_(false){
  }

  void listen(boost::interprocess::mode_t m) {
    this->daemon_ = true;
    if(!this->shm_init_) {
      shared_memory_object::remove(this->name_.c_str());
      this->shm_ = shared_memory_object(boost::interprocess::open_or_create, this->name_.c_str() , m);
      this->shm_.truncate(sizeof(t_sync_state) + sizeof(t_ipc_semaphore));
      //Map the whole shared memory in this process
      this->region_ = mapped_region(shm_, m);

      /**
       * initalize the message_queue
       */
      std::string mq_name = this->name_ + "_mq";
      //Erase previous message queue
      t_msg_queue::remove(mq_name.c_str());
      this->mq_ = new t_msg_queue(boost::interprocess::open_or_create, mq_name.c_str(), T_size, sizeof(T_char_type));
      this->shm_init_ = true;
    }

    // the shared ring buffer does a three way handshake to establish a connection

    /**
     *  initialize the sync_state.
     *  defaults to FIN which makes all clients wait for a SIN.
     */
    t_sync_state* addr = static_cast<t_sync_state*>(this->region_.get_address());
    this->state_ = new (addr) t_sync_state;
    t_sync_state r;

    /**
     * initialize the mutex (really semaphore).
     */
    this->mutex_ = new ( static_cast<void *>(addr+1) ) t_ipc_semaphore(1);

    /**
     * promote handshake by writing SIN
     */
    (*this->state_) = SIN_;

    /**
     * wait for one or more clients acknowledge.
     */
    while((r = *this->state_) != ACK_) {
      sleep(milliseconds(10));
    }

    /**
     * we definitely have at least one client.
     * allow client(s) to access the mutex.
     */
    *this->state_ = FIN_;
  }

  void connect(boost::interprocess::mode_t m) {
    this->daemon_ = false;

    for(;;) {
      shm_ = shared_memory_object(boost::interprocess::open_only, this->name_.c_str(), m);
      shm_.truncate(sizeof(t_sync_state) + sizeof(t_ipc_semaphore));
      //Map the whole shared memory in this process
      region_ = mapped_region(shm_, m);

      t_sync_state* addr = static_cast<t_sync_state*> (region_.get_address());
      this->state_ = static_cast<volatile t_sync_state*> (addr);
      t_sync_state r;
      /**
     *  wait for the daemon to promote SIN.
     */
    while((r = *this->state_) != SIN_) {
      sleep(milliseconds(10));
    }

    /**
     * client(s) need to make sure there's actually a daemon listening on
     * the other end as well as the daemon should be informed of its (their)
     * presence.
     */

    *this->state_ = ACK_;

    /**
     * wait for the daemon to finish the handshake with a FIN.
     */
    while((r = *this->state_) == ACK_) {
      sleep(milliseconds(10));
    }

    if(r != FIN_) {
      continue;
    }
    /**
     * there's definitely a daemon on the other end.
     * acquire access to the ring buffer.
     */
    this->mutex_ = static_cast<t_ipc_semaphore*> (static_cast<void *>(addr + 1));
    if(!this->mutex_->try_wait())
      continue; // it's not our turn. start over.

    std::string mq_name = this->name_ + "_mq";
    this->mq_ = new t_msg_queue(boost::interprocess::open_only, mq_name.c_str());
    break;
    }
  }

  bool isOpen() {
    return *this->state_ == FIN_;
  }

  void close() {
    if(!this->daemon_) {
      *this->state_ = CLS_;
      this->mutex_->post();
    } else {
      *this->state_ = CLS_;
    }
  }

  virtual ~basic_ringbuffer_connection() {
    this->close();
  }
};

typedef basic_ringbuffer_connection<4096, char> ringbuffer_connection;
#endif /* IPC_HPP_ */
