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

using boost::interprocess::shared_memory_object;
using boost::interprocess::interprocess_semaphore;
using boost::interprocess::mapped_region;
using boost::interprocess::create_only_t;
using boost::interprocess::open_only_t;

template <std::streamsize T_size, typename T_char_type, typename T_traits = std::char_traits<T_char_type> >
struct basic_ringbuf {
  typedef typename T_traits::int_type int_type;

  //Semaphores to protect and synchronize access
  boost::interprocess::interprocess_semaphore
  lock_, mutex_, nempty_, nstored_;

  std::streamsize rear_ = 0, front_ = 0;
  T_char_type buffer_[T_size];

  basic_ringbuf() :
    lock_(1),
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

  basic_shared_ringbuf(create_only_t c, const char* name, boost::interprocess::mode_t m) :
      name(name) {
    shared_memory_object::remove(name);
    shm = shared_memory_object(c, name, m);
    shm.truncate(sizeof(t_ringbuf));
    //Map the whole shared memory in this process
    region = mapped_region(shm, m);

    this->rbuf = new (region.get_address()) t_ringbuf;
  }

  basic_shared_ringbuf(boost::interprocess::open_only_t c, const char* name, boost::interprocess::mode_t m) :
      name(name) {
    shm = shared_memory_object(c, name, m);
    shm.truncate(sizeof(t_ringbuf));
    //Map the whole shared memory in this process
    region = mapped_region(shm, m);

    this->rbuf = static_cast<t_ringbuf*>(region.get_address());
    this->rbuf->lock_.wait();
  }

  virtual ~basic_shared_ringbuf() {
    this->rbuf->lock_.post();
  }
};

//typedef basic_shared_ringbuf<4096, char> ringbuf;
typedef basic_shared_ringbuf<4096, char> shared_ringbuf;
#endif /* IPC_HPP_ */
