#ifndef IPC_HPP_
#define IPC_HPP_

#include <cstring>
#include <functional>
#include <string>
#include <boost/exception/all.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
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

template <std::streamsize T_size, typename T_char_type, typename T_traits = std::char_traits<T_char_type> >
struct basic_ringbuf {
  typedef typename T_traits::int_type t_int_type;
  typedef boost::interprocess::interprocess_semaphore ipc_semaphore;

  //Semaphores to protect and synchronize access
  ipc_semaphore mutex_, nempty_, nstored_;

  std::streamsize rear_ = 0, front_ = 0;
  t_int_type buffer_[T_size];

  basic_ringbuf() :
    mutex_(1),
    nempty_(T_size),
    nstored_(0)
  {}

  void putc(const t_int_type& d) {
    this->nempty_.wait();
    this->mutex_.wait();

    this->buffer_[++this->front_ % T_size ] = d;

    this->mutex_.post();
    this->nstored_.post();
  }

  void putc(const T_char_type& c) {
    this->putc(T_traits::to_int_type(c));
  }

  const t_int_type getc() {
    try {
    this->nstored_.wait();
    } catch (boost::interprocess::interprocess_exception &ex) {
      std::cerr << ex.get_error_code() << std::endl;
      std::cerr << ex.get_native_error() << std::endl;
      exit(2);
    }
      this->mutex_.wait();
      const t_int_type & c = this->buffer_[++this->rear_ % T_size];
      this->mutex_.post();
      this->nempty_.post();
      return c;
  }
};

template <std::streamsize T_size, typename T_char_type, typename T_traits = std::char_traits<T_char_type> >
class basic_ringbuffer_connection {

private:
  typedef boost::interprocess::string t_string;
  typedef typename T_traits::int_type t_int_type;

  typedef uint8_t t_sync_state;
  typedef boost::interprocess::interprocess_semaphore t_ipc_semaphore;
  typedef basic_ringbuf<T_size, T_char_type, T_traits> t_ringbuf;

  shared_memory_object shm_;
  mapped_region region_;

  const std::string name_;

  volatile t_sync_state* state_;
  t_ipc_semaphore*  mutex_;

  //the underlying ringbuffer manages memory and synchronisation itself
  t_ringbuf* rbuf_;
  bool daemon_;

  static const t_sync_state FIN_ = 0;
  static const t_sync_state ACK_ = 1;
  static const t_sync_state SIN_ = 2;
public:
  struct connection_error: virtual boost::exception { };

  typedef basic_ringbuffer_connection<T_size, T_char_type, T_traits> type;
  typedef T_traits t_char_traits;

  struct Sink {
    typedef T_char_type  char_type;
    typedef boost::iostreams::sink_tag  category;

    type* t;
    Sink(type* t): t(t)
    {}

    std::streamsize write(const T_char_type* s, std::streamsize n) {
      int i = 0;
      for(; i < n; i++) {
        t->rbuf_->putc(*s);
        ++s;
      }
      return i;
    }
  };

  struct Source {
      typedef T_char_type char_type;
      typedef boost::iostreams::source_tag  category;
      type* t;
      Source(type* t): t(t)
      {}

      std::streamsize read(char* s, std::streamsize n) {
        t_int_type d;
        int i = 0;
        for(; i < n; i++) {
          if((d = t->rbuf_->getc()) == T_traits::eof())
            break;

          *s = T_traits::to_char_type(d);
          ++s;
        }
        return i;
      }

      std::streamsize readLn(std::string& s) {
        t_int_type d;
        int i = 0;
        s.clear();
        while((d = t->rbuf_->getc()) != T_traits::eof() && T_traits::to_char_type(d) != '\n') {
          s += T_traits::to_char_type(d);
          ++i;
        }
        return i;
      }
  };

  typedef boost::iostreams::stream<Sink> ostream;
  typedef boost::iostreams::stream<Source> istream;


/*
  syn >> 1
mutex >> 2
         1 >> syn
         1 << ack
               reread
  ack << 1 >> ack
         1 >> fin
               reread
  fin >> 1
         1 >> fin
         2 >> mutex
*/

  basic_ringbuffer_connection(create_only_t c, const char* name, boost::interprocess::mode_t m) :
      name_(name),
      mutex_(NULL),
      daemon_(true) {

    shared_memory_object::remove(name);
    this->shm_ = shared_memory_object(c, name, m);
    this->shm_.truncate(sizeof(t_sync_state) + sizeof(t_ipc_semaphore) + sizeof(t_ringbuf));
    //Map the whole shared memory in this process
    this->region_ = mapped_region(shm_, m);

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
     * initalize the ringbuffer
     */

    this->rbuf_ = new ( static_cast<void *>(this->mutex_+1)) t_ringbuf;

    /**
     * promote handshake by writing SIN
     */
    (*this->state_) = SIN_;
    std::cerr << "server init: " << name << std::endl;

    /**
     * wait for one or more clients acknowledge.
     */
    while((r = *this->state_) != ACK_) {
      std::cerr << "server wait ack: " << r << std::endl;
      boost::this_thread::sleep(boost::posix_time::milliseconds(1));
    }

    /**
     * we definitely have at least one client.
     * allow client(s) to access the mutex.
     */
    *this->state_ = FIN_;

    std::cerr << "server fin: " << name << std::endl;
  }

  basic_ringbuffer_connection(boost::interprocess::open_only_t c, const char* name, boost::interprocess::mode_t m) :
      name_(name),
      mutex_(NULL),
      daemon_(false) {

    shm_ = shared_memory_object(c, name, m);
    shm_.truncate(sizeof(t_sync_state) + sizeof(t_ipc_semaphore) + sizeof(t_ringbuf));
    //Map the whole shared memory in this process
    region_ = mapped_region(shm_, m);

    t_sync_state* addr = static_cast<t_sync_state*> (region_.get_address());
    this->state_ = static_cast<volatile t_sync_state*> (addr);
    t_sync_state r;

    std::cerr << "client init: " << name << std::endl;

    for(;;) {
    /**
     *  wait for the daemon to promote SIN.
     */
    while((r = *this->state_) != SIN_) {
      std::cerr << "client wait sin:" << r << std::endl;
      boost::this_thread::sleep(boost::posix_time::milliseconds(10));
    }

    /**
     * client(s) need to make sure there's actually a daemon listening on
     * the other end as well as the daemon should be informed of its (their)
     * presence.
     */

    std::cerr << "write ack" << std::endl;
    *this->state_ = ACK_;

    /**
     * wait for the daemon to finish the handshake with a FIN.
     */
    for(size_t retries = 20; retries > 0 && (r = *this->state_) == ACK_; --retries) {
      std::cerr << "client wait fin: " << r << std::endl;
      boost::this_thread::sleep(boost::posix_time::milliseconds(10));
    }

    if(r != FIN_) {
      throw connection_error(); // daemon didn't finalize the connection. abort
    }
    /**
     * there's definitely a daemon on the other end.
     * acquire access to the ring buffer.
     */
    this->mutex_ = static_cast<t_ipc_semaphore*> (static_cast<void *>(addr + 1));
    if(!this->mutex_->try_wait())
      continue; // it's not our turn. start over.

    this->rbuf_ = static_cast<t_ringbuf*>(static_cast<void *>(this->mutex_ + 1));
    std::cerr << "client done" << std::endl;
    break;
    }
  }

  size_t available() {
    return this->rbuf_->front_ - this->rbuf_->rear_;
  }

  void close() {
    if(!this->daemon_)
      this->mutex_->post();
  }

  virtual ~basic_ringbuffer_connection() {
    this->close();
  }
};

typedef basic_ringbuffer_connection<4096, char> ringbuffer_connection;
#endif /* IPC_HPP_ */
