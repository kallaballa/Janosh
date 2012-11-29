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

template <std::streamsize T_size, typename T_char_type, typename T_traits = std::char_traits<T_char_type> >
struct basic_ringbuf {
  typedef typename T_traits::int_type int_type;

  //Semaphores to protect and synchronize access
  boost::interprocess::interprocess_semaphore
     mutex_, nempty_, nstored_;

  std::streamsize rear_ = 0, front_ = 0;
  T_char_type buffer_[T_size];


  basic_ringbuf() :
    mutex_(1),
    nempty_(T_size),
    nstored_(0)
  {}

  inline bool putc(const T_char_type & c) {
    this->nempty_.wait();
    this->mutex_.wait();

    this->buffer_[++this->front_ % T_size ] = c;

    this->mutex_.post();
    this->nstored_.post();
    return true;
  }

  inline const int_type getc() {
    this->nstored_.wait();
    this->mutex_.wait();
    const T_char_type & c = this->buffer_[++this->rear_ % T_size];
    this->mutex_.post();
    this->nempty_.post();
    return T_traits::to_int_type(c);
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
    const t_sync_state OPEN_ = 0;
    const t_sync_state ACCEPT_ = 1;
    const t_sync_state OFFER_ = 2;
    const t_sync_state CLOSE_ = 3;

    t_sync_state state_;
    t_ipc_mutex server_mutex_;
    t_ipc_mutex client_mutex_;
    t_ringbuf rbuf_;

    shared_buffer() :
        state_(CLOSE_), server_mutex_(0), client_mutex_(0), rbuf_() {
    }

    void close() {
      this->state_= CLOSE_;
    }

    bool isOpen() {
      return this->state_ == OPEN_;
    }

    std::streamsize available() {
      return this->rbuf_.front_ - this->rbuf_.rear_;
    }

    inline bool putc(const T_char_type & c) {
      return rbuf_.putc(c);
    }

    inline const t_ringbuf::int_type getc() {
      return rbuf_.getc();
    }

    void setState(const t_sync_state& s) {
      this->state_ = s;
    }

    void connect() {
      typename shared_buffer::t_sync_state r;

      //wait for the server to accept
      shared_buffer::client_mutex_.wait();

      if (shared_buffer::state_ != shared_buffer::OPEN_)
        throw new connection_error();


      shared_buffer::server_mutex_.post();
    }

    void listen() {
      shared_buffer::setState(shared_buffer::OPEN_);
      shared_buffer::client_mutex_.post();
      shared_buffer::server_mutex_.wait();
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
      if(!t->isOpen())
        return 0;

      for(int i = 0; i < n; ++i) {
        t->putc(*s);
        ++s;
      }

      if(n == 0)
        std::cerr << std::endl;

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
      t_int_type c;
      std::streamsize num = 0;
      do {
        try {
          num = std::min(t->available(),
              boost::lexical_cast<std::streamsize>(n));

          if (num > 0) {
            for (std::streamsize i = 0; i < num; i++) {
              *s = T_char_traits::to_char_type(c = t->getc());
              ++s;
            }

            if (num == 0)
              std::cerr << std::endl;

            return num;
          }
        } catch (std::exception& ex) {
          std::cerr << "what: " << ex.what() << std::endl;
          std::cout << "what: " << ex.what() << std::endl;
          std::cout.flush();
          std::cerr.flush();
        }
      } while (t->isOpen() || num > 0);

      return 0;
    }
  };

  typedef boost::iostreams::stream<Sink> ostream;
  typedef boost::iostreams::stream<Source> istream;

  basic_ringbuffer_connection(const char* name) :
      name_(name),
      shm_init_(false){
  }

  void listen(boost::interprocess::mode_t m) {
    this->daemon_ = true;

    if(!this->shm_init_) {
//    if(1) {
      shared_memory_object::remove(this->name_.c_str());
      this->shm_ = shared_memory_object(boost::interprocess::open_or_create, this->name_.c_str() , m);
      this->shm_.truncate(sizeof(shared_buffer));
      //Map the whole shared memory in this process
      this->region_ = mapped_region(shm_, m);
      this->shm_init_ = true;

      this->shm_buf_ = new (this->region_.get_address()) shared_buffer;
    }
    this->shm_buf_->listen();
  }

  void connect(boost::interprocess::mode_t m) {
    this->daemon_ = false;
    shm_ = shared_memory_object(boost::interprocess::open_only, this->name_.c_str(), m);
    shm_.truncate(sizeof(shared_buffer));
    //Map the whole shared memory in this process
    region_ = mapped_region(shm_, m);

    this->shm_buf_ = static_cast<shared_buffer*>(region_.get_address());
    this->shm_buf_->connect();
  }

  typename type::istream* make_istream() {
    return new istream(this->shm_buf_);
  }

  typename type::ostream* make_ostream() {
    return new ostream(this->shm_buf_);
  }

  virtual ~basic_ringbuffer_connection() {
  }

  void close() {
    this->shm_buf_->close();
  }

  bool isOpen() {
    return this->shm_buf_->isOpen();
  }
};

typedef basic_ringbuffer_connection<4096, char> ringbuffer_connection;
#endif /* IPC_HPP_ */
