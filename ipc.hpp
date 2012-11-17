#ifndef IPC_HPP_
#define IPC_HPP_

#include <cstring>
#include <functional>
#include <string>
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
  typedef typename T_traits::int_type int_type;
  typedef boost::interprocess::interprocess_semaphore ipc_semaphore;

  //Semaphores to protect and synchronize access
  ipc_semaphore mutex_, nempty_, nstored_;

  std::streamsize rear_ = 0, front_ = 0;
  T_char_type buffer_[T_size];

  basic_ringbuf() :
    mutex_(1),
    nempty_(T_size),
    nstored_(0)
  {}

  bool putc(const T_char_type & c) {
    if(this->nempty_.try_wait()) {
      this->mutex_.wait();

      this->buffer_[++this->front_ % T_size ] = c;

      this->mutex_.post();
      this->nstored_.post();
      return true;
    }
    return false;
  }

  const int_type getc() {
    if(this->nstored_.try_wait()) {
      this->mutex_.wait();
      const T_char_type & c = this->buffer_[++this->rear_ % T_size];
      this->mutex_.post();
      this->nempty_.post();
      return T_traits::to_int_type(c);
    }

    return T_traits::eof();
  }

  const int_type getc_block() {
    this->nstored_.wait();
    this->mutex_.wait();
    const T_char_type & c = this->buffer_[++this->rear_ % T_size];
    this->mutex_.post();
    this->nempty_.post();
    return T_traits::to_int_type(c);
  }
};

template <std::streamsize T_size, typename T_char_type, typename T_traits = std::char_traits<T_char_type> >
class basic_shared_ringbuf {

private:
  typedef boost::interprocess::interprocess_semaphore ipc_semaphore;
  typedef basic_shared_ringbuf<T_size, T_char_type, T_traits> type;
  typedef basic_ringbuf<T_size, T_char_type, T_traits> t_ringbuf;

  const std::string name;
  t_ringbuf* rbuf;
public:
  typedef T_traits t_traits;

  struct Sink {
    typedef T_char_type  char_type;
    typedef boost::iostreams::sink_tag  category;

    type* t;
    Sink(type* t): t(t)
    {}

    std::streamsize write(const T_char_type* s, std::streamsize n) {
      for(int i = 0; i < n; i++) {
        if(!t->rbuf->putc(*s))
          return i;
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
        int_type d;
        for(int i = 0; i < n; i++) {
          if((d = t->rbuf->getc()) == T_traits::eof()) {
            if(i == 0)
              d = t->rbuf->getc_block();
            else
              return i;
          }

          *s = T_traits::to_char_type(d);
          ++s;
        }
        return n;
      }
  };

  typedef boost::interprocess::string string;
  typedef typename T_traits::int_type int_type;
  typedef boost::iostreams::stream<Sink> ostream;
  typedef boost::iostreams::stream<Source> istream;

  shared_memory_object shm;
  mapped_region region;
  ipc_semaphore*  mutex_;

  enum sync_state {
    FIN,
    ACK,
    SIN
  };

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

  basic_shared_ringbuf(create_only_t c, const char* name, boost::interprocess::mode_t m) :
      name(name) {
    shared_memory_object::remove(name);
    shm = shared_memory_object(c, name, m);
    shm.truncate(sizeof(sync_state) + sizeof(ipc_semaphore) + sizeof(t_ringbuf));
    //Map the whole shared memory in this process
    region = mapped_region(shm, m);

    sync_state* s = new (region.get_address()) sync_state;
    sync_state r;
  //  this->mutex_ = new ( ((char*)region.get_address()) + sizeof(sync_state)) ipc_semaphore(1);
    *s = SIN;
    std::cerr << "server init" << std::endl;
    while((r = *s) != ACK) {
      std::cerr << "server wait ack: " << r << std::endl;
      boost::this_thread::sleep(boost::posix_time::milliseconds(1));
    }
    *s = FIN;
    std::cerr << "server fin" << std::endl;

    this->rbuf = new (((char*)region.get_address()) + sizeof(sync_state) + sizeof(ipc_semaphore)) t_ringbuf;
  }

  basic_shared_ringbuf(boost::interprocess::open_only_t c, const char* name, boost::interprocess::mode_t m) :
      name(name),
      mutex_(NULL){
    shm = shared_memory_object(c, name, m);
    shm.truncate(sizeof(sync_state) + sizeof(ipc_semaphore) + sizeof(t_ringbuf));
    //Map the whole shared memory in this process
    region = mapped_region(shm, m);

    sync_state* s = static_cast<sync_state*> (region.get_address());
    sync_state r;
    std::cerr << "client init" << std::endl;
    while((r = *s) != SIN) {
      std::cerr << "client wait sin:" << r << std::endl;
      boost::this_thread::sleep(boost::posix_time::milliseconds(1));
    }

    std::cerr << "set ack: " << r << std::endl;
    *s = ACK;

    while((r = *s) != FIN || r == ACK) {
      std::cerr << "wait fin: " << r << std::endl;
      boost::this_thread::sleep(boost::posix_time::milliseconds(1));
    }

    assert(r != SIN);

    ipc_semaphore* sema = static_cast<ipc_semaphore*> ((void*)((char*)region.get_address() + sizeof(sync_state)));
    sema->wait();
    this->rbuf = static_cast<t_ringbuf*>((void*)((char*)region.get_address() + sizeof(sync_state) + sizeof(ipc_semaphore)));
  }

  virtual ~basic_shared_ringbuf() {
    if(this->mutex_)
      this->mutex_->post();
  }
};

typedef basic_shared_ringbuf<4096, char> shared_ringbuf;
#endif /* IPC_HPP_ */
