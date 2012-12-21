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
#include <boost/chrono.hpp>
#include "logger.hpp"
#include <time.h>
#include <sys/timeb.h>

using boost::interprocess::shared_memory_object;
using boost::interprocess::interprocess_semaphore;
using boost::interprocess::mapped_region;
using boost::interprocess::create_only_t;
using boost::interprocess::open_only_t;
using boost::this_thread::sleep;
using boost::posix_time::milliseconds;

static void debug(const std::string& s) {
#ifdef JANOSH_DEBUG
  timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);

  std::cerr << "(" <<  ts.tv_sec << "," << ts.tv_nsec << ")" << s << std::endl;
#endif
}

template <std::streamsize T_size, typename T_char_type, typename T_traits = std::char_traits<T_char_type> >
struct basic_ringbuf {
  typedef typename T_traits::int_type int_type;

  //Semaphores to protect and synchronize access
  boost::interprocess::interprocess_semaphore
     mutex_, nempty_, nstored_;

  std::streamsize rear_ = 0, front_ = 0;
  T_char_type buffer_[T_size];
  char hi;


  basic_ringbuf() :
    mutex_(1),
    nempty_(T_size),
    nstored_(0)
  {}

  inline bool putc(const T_char_type & c) {
    nempty_.wait();
    mutex_.wait();

    buffer_[++front_ % T_size ] = c;

    mutex_.post();
    nstored_.post();
    return true;
  }

  inline const int_type getc() {
    nstored_.wait();
    mutex_.wait();
    const T_char_type & c = buffer_[++rear_ % T_size];
    mutex_.post();
    nempty_.post();
    return T_traits::to_int_type(c);
  }

  void reset() {
    debug("reset");
    while(mutex_.try_wait());
    mutex_.post();

    while(nstored_.try_wait());
    while(nempty_.try_wait());

    for(size_t i = 0; i < T_size; ++i) {
      nempty_.post();
    }
    rear_ = 0;
    front_ = 0;
  }
};

template <std::streamsize T_size, typename T_char_type, typename T_char_traits = std::char_traits<T_char_type> >
class basic_ringbuffer_connection {

private:
  typedef basic_ringbuf<4096, char> t_ringbuf;
  typedef typename T_char_traits::int_type t_int_type;
  typedef boost::interprocess::string t_string;

  shared_memory_object shm_;
  mapped_region region_;

  bool daemon_;
  const std::string name_;

  struct shared_buffer : public basic_ringbuf<T_size, char>{
    typedef uint8_t t_sync_state;
    typedef boost::interprocess::interprocess_semaphore t_ipc_mutex;
    const t_sync_state ACCEPT_ = 1;
    const t_sync_state OFFER_ = 2;
    const t_sync_state CLOSE_ = 3;
    const t_sync_state OPEN_ = 4;

    volatile t_sync_state state_;
    t_ipc_mutex server_mutex_;
    t_ipc_mutex client_mutex_;
    t_ringbuf rbuf_;

    shared_buffer() :
        state_(CLOSE_), server_mutex_(0), client_mutex_(0), rbuf_() {
    }

    void close() {
      setState(CLOSE_);
    }

    bool isOpen() {
      return this->state_ == OPEN_;
    }

    std::streamsize available() {
      return this->rbuf_.front_ - this->rbuf_.rear_;
    }

    std::streamsize free() {
      return T_size - this->available();
    }


    inline bool putc(const T_char_type & c) {
      return rbuf_.putc(c);
    }

    inline const t_ringbuf::int_type getc() {
      debug("get:" + boost::lexical_cast<std::string>(this->available()));
      return rbuf_.getc();
    }

    void setState(const t_sync_state& s) {
      this->state_ = s;
    }

    t_sync_state getState() {
      return this->state_;
    }

    void connect() {
      debug("buf wait");
      typename shared_buffer::t_sync_state r;
      //wait for the server to accept

      shared_buffer::server_mutex_.post();
      shared_buffer::client_mutex_.wait();
      if(getState() != OPEN_)
        throw connection_error();

      debug("buf connect");
    }

    void listen() {
      debug("buf wait");
      shared_buffer::server_mutex_.wait();
      setState(shared_buffer::OPEN_);
      this->rbuf_.reset();
      shared_buffer::client_mutex_.post();
      debug("buf listen");
    }

    void finalize() {
      close();
      while(client_mutex_.try_wait());
      client_mutex_.post();
    }
 };

public:
  shared_buffer* shm_buf_;
  bool shm_init_;
  struct connection_error: virtual boost::exception { };

  typedef std::char_traits<T_char_type> t_char_traits;
  typedef basic_ringbuffer_connection<T_size, T_char_type> type;

  struct Sink {
    typedef T_char_type  char_type;
    struct category
            : boost::iostreams::sink_tag
            { };

    shared_buffer* t;
    Sink(shared_buffer* t): t(t) {}

    std::streamsize write(const T_char_type* s, std::streamsize n) {
      size_t retries = 0;
      while(t->free() == 0 && t->isOpen()) {
        if(++retries > 20) {
          debug("sink abort");
          t->close();
          return 0;
        }
        debug("sink wait");
        boost::this_thread::sleep(milliseconds(5));
      }

      if(!t->isOpen()) {
        debug("sink term");
        t->close();
        return 0;
      }

      n = std::min(t->free(), n);
      debug("sink write:" + boost::lexical_cast<std::string>(n) + "/" + boost::lexical_cast<std::string>(t->free()));
      for(int i = 0; i < n; ++i) {
        debug("put" + boost::lexical_cast<std::string>(i) + "/"  + boost::lexical_cast<std::string>(t->free()) );
        t->putc(*s);
        ++s;
      }
      debug("sink done");

      return n;
    }
  };

  struct Source {
    typedef T_char_type char_type;
    struct category : boost::iostreams::source_tag
    {};
    shared_buffer* t;
    Source(shared_buffer* t): t(t)
    {}

    std::streamsize read(char* s, std::streamsize n) {
      debug(("source read: "
          + boost::lexical_cast<std::string>(t->isOpen())
          + "/" +
          boost::lexical_cast<std::string>(t->available())));
      t_int_type c;
      std::streamsize num = 0;
     // size_t retries = 0;

      do {
          num = std::min(t->available(),
              boost::lexical_cast<std::streamsize>(n));

          if (num > 0) {
            for (std::streamsize i = 0; i < num; i++) {
              *s = T_char_traits::to_char_type(c = t->getc());
              ++s;
            }

            return num;
          } else {
            debug("source wait: " + t->available());
/*            if(++retries > 20) {
              debug("source abort");
              t->close();
              return 0;
            }*/
            boost::this_thread::sleep(milliseconds(5));
          }
      } while (t->isOpen() || t->available() > 0);
      debug(("source read exit: "
          + boost::lexical_cast<std::string>(t->isOpen())
          + "/" +
          boost::lexical_cast<std::string>(t->available())));

      return 0;
    }
  };

  typedef boost::iostreams::stream<Sink> ostream;
  typedef boost::iostreams::stream<Source> istream;

  typename type::istream* in_;
  typename type::ostream* out_;

  basic_ringbuffer_connection(const char* name) :
      name_(name),
      shm_init_(false),
      in_(NULL),
      out_(NULL){
  }


  virtual ~basic_ringbuffer_connection() {
  }

  void listen(boost::interprocess::mode_t m) {
    this->daemon_ = true;

    if(!this->shm_init_) {
      debug("shm_init");

//      shared_memory_object::remove(this->name_.c_str());
      this->shm_ = shared_memory_object(boost::interprocess::open_or_create, this->name_.c_str() , m);
      this->shm_.truncate(sizeof(shared_buffer));
      //Map the whole shared memory in this process
      this->region_ = mapped_region(shm_, m);
      this->shm_buf_ = new (this->region_.get_address()) shared_buffer;

      this->shm_init_ = true;
    }

    if(this->in_)
      delete this->in_;
    if(this->out_)
      delete this->out_;

    this->in_ = new istream(shm_buf_);
    this->out_ = new ostream(shm_buf_);

    this->shm_buf_->listen();
  }

  void connect(boost::interprocess::mode_t m) {
    this->daemon_ = false;
    shm_ = shared_memory_object(boost::interprocess::open_only, this->name_.c_str(), m);
    shm_.truncate(sizeof(shared_buffer));
    //Map the whole shared memory in this process
    region_ = mapped_region(shm_, m);

    this->shm_buf_ = static_cast<shared_buffer*>(region_.get_address());

    if(this->in_)
      delete this->in_;
    if(this->out_)
      delete this->out_;

    this->in_ = new istream(shm_buf_);
    this->out_ = new ostream(shm_buf_);

    this->shm_buf_->connect();
  }

  void remove() {
    if(this->daemon_) {
      this->shm_buf_->finalize();
      shared_memory_object::remove(this->name_.c_str());
    }
  }

  typename type::istream* make_istream() {
    return this->in_;
  }

  typename type::ostream* make_ostream() {
    return this->out_;
  }

  void close() {
    debug("close: " + this->name_);
    this->shm_buf_->close();
  }

  bool isOpen() {
    return this->shm_buf_->isOpen();
  }
};

typedef basic_ringbuffer_connection<4096, char> ringbuffer_connection;
#endif /* IPC_HPP_ */
