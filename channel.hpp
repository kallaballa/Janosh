
#ifndef REQUEST_HPP_
#define REQUEST_HPP_

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <iostream>
#include <boost/iostreams/device/back_inserter.hpp>
#include "logger.hpp"
#include "ipc.hpp"

using std::string;
namespace janosh {
  class Channel {
  private:
    shared_ringbuf* shm_rbuf_in_;
    shared_ringbuf* shm_rbuf_out_;
    shared_ringbuf::istream* in_;
    shared_ringbuf::ostream* out_;
    bool daemon_;
  public:
    Channel(bool daemon=false) : daemon_(daemon){
      const char* user = getenv ("USER");
      if (user==NULL) {
        LOG_ERR_MSG("Can't find environment variable.", "USER");
        exit(1);
      }

      string nameIn = "JanoshIPC_in" + string(user);
      string nameOut = "JanoshIPC_out" + string(user);

      if(daemon) {
        this->shm_rbuf_in_ = new shared_ringbuf(boost::interprocess::create_only, nameIn.c_str(), boost::interprocess::read_write);
        this->shm_rbuf_out_ = new shared_ringbuf(boost::interprocess::create_only, nameOut.c_str(), boost::interprocess::read_write);
        this->in_ = new shared_ringbuf::istream(shm_rbuf_in_);
        this->out_ = new shared_ringbuf::ostream(shm_rbuf_out_);
      } else {
        //swap in and out channel
        this->shm_rbuf_out_ = new shared_ringbuf(boost::interprocess::open_only, nameIn.c_str(), boost::interprocess::read_write);
        this->shm_rbuf_in_ = new shared_ringbuf(boost::interprocess::open_only, nameOut.c_str(), boost::interprocess::read_write);
        this->in_ = new shared_ringbuf::istream(shm_rbuf_in_);
        this->out_ = new shared_ringbuf::ostream(shm_rbuf_out_);
      }
    }

    shared_ringbuf::istream& in() {
      return *this->in_;
    }

    shared_ringbuf::ostream& out() {
      return *this->out_;
    }

    void writeln(const string& s) {
      out() << s << std::endl;
    }

    void send(bool success) {
      if(this->daemon_) {
        this->out() << "{?endofjanosh?}" << std::endl;
        this->out() << (success ? 1 : 0) << std::endl;
      } else {
        //use -d to terminate list of arguments since a client should never pass -d
        this->writeln("-d");
      }
    }

    bool receive(std::vector<string>& v) {
      if(this->daemon_) {
        string l;
          while(!std::getline(this->in(), l)) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(5));
          }
          v.push_back(l);
          while(std::getline(this->in(), l) && l != "-d") {
            v.push_back(l);
          }
          return true;
      } else {
        string l;
        while(std::getline(this->in(), l) && l != "{?endofjanosh?}") {
          v.push_back(l);
        }
        std::getline(this->in(), l);
        return boost::lexical_cast<size_t>(l) ? 0 : 1;
      }
    }

    bool receive(std::ostream& os) {
      if(this->daemon_) {
        string l;
          while(!std::getline(this->in(), l)) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(5));
          }
          os << l << std::endl;
          while(std::getline(this->in(), l) && l != "-d") {
            os << l << std::endl;
          }
          return true;
      } else {
        string l;
        while(std::getline(this->in(), l) && l != "{?endofjanosh?}") {
          os << l << std::endl;
        }
        std::getline(this->in(), l);
        return boost::lexical_cast<size_t>(l) ? 0 : 1;
      }
    }
 };
}
#endif /* REQUEST_HPP_ */
