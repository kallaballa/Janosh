#include <stdint.h>
#include <stddef.h>
#include <boost/iostreams/device/back_inserter.hpp>
#include "logger.hpp"
#include "channel.hpp"

using std::string;
namespace janosh {
    Channel::Channel() : shm_rbuf_in_(NULL), shm_rbuf_out_(NULL) {}

    void Channel::accept() {
      const char* user = getenv ("USER");
      if (user==NULL) {
        LOG_ERR_MSG("Can't find environment variable.", "USER");
        exit(1);
      }
      string nameIn = "JanoshIPC_in" + string(user);
      string nameOut = "JanoshIPC_out" + string(user);

      this->daemon_ =  true;

      if(!this->shm_rbuf_in_)
        this->shm_rbuf_in_ = new ringbuffer_connection(nameIn.c_str());

      if(!this->shm_rbuf_out_)
        this->shm_rbuf_out_ = new ringbuffer_connection(nameOut.c_str());

      boost::thread t([&](){
        this->shm_rbuf_out_->listen(boost::interprocess::read_write);
      });

      while(!this->shm_rbuf_in_->shm_init_ && t.timed_join(boost::posix_time::millisec(5)))
      {}

      this->shm_rbuf_in_->listen(boost::interprocess::read_write);
      t.join();
      this->in_ = new ringbuffer_connection::istream(shm_rbuf_in_);
      this->out_ = new ringbuffer_connection::ostream(shm_rbuf_out_);
    }

    void Channel::connect() {
      const char* user = getenv ("USER");
      if (user==NULL) {
        LOG_ERR_MSG("Can't find environment variable.", "USER");
        exit(1);
      }
      string nameIn = "JanoshIPC_in" + string(user);
      string nameOut = "JanoshIPC_out" + string(user);

      //swap in and out channel
      this->shm_rbuf_in_ = new ringbuffer_connection(nameOut.c_str());
      this->shm_rbuf_out_ = new ringbuffer_connection(nameIn.c_str());

      this->shm_rbuf_out_->connect(boost::interprocess::read_write);
      this->shm_rbuf_in_->connect(boost::interprocess::read_write);

      this->in_ = new ringbuffer_connection::istream(shm_rbuf_in_);
      this->out_ = new ringbuffer_connection::ostream(shm_rbuf_out_);
    }

    Channel::~Channel() {
      this->shm_rbuf_in_->close();
      this->shm_rbuf_out_->close();
    }

    std::istream& Channel::in() {
      return *this->in_;
    }

    std::ostream& Channel::out() {
      return *this->out_;
    }

    void Channel::writeln(const string& s) {
      out() << s << std::endl;
    }

    void Channel::flush(bool success) {
      if(this->daemon_) {
        this->out() << "{?endofjanosh?}" << std::endl;
        this->out() << (success ? 1 : 0) << std::endl;
      } else {
        //use -d to terminate list of arguments since a client should never pass -d
        this->writeln("-d");
      }
      out().flush();
    }


    bool Channel::receive(std::vector<string>& v) {
      if(this->daemon_) {
        string l;
        while(std::getline(this->in(), l)) {
          std::cerr << "read" << l << std::endl;
          if(l == "-d")
            break;
          else
            v.push_back(l);
        }

        return true;
      } else {
        string l;
        while(std::getline(this->in(), l)) {
          if(l == "{?endofjanosh?}")
            break;
          else
            v.push_back(l);
        }
        std::getline(this->in(), l);
        return boost::lexical_cast<size_t>(l) ? 0 : 1;
      }
    }

    bool Channel::receive(std::ostream& os) {
      if(this->daemon_) {
        string l;

        while(std::getline(this->in(), l)) {
          if(l == "-d")
            break;
          else
            os << l << std::endl;
        }
        return true;
      } else {
        string l;

        while(std::getline(this->in(), l)) {
          if(l == "{?endofjanosh?}")
            break;
          else
            os << l << std::endl;
        }

        std::getline(this->in(), l);
        return boost::lexical_cast<size_t>(l) ? 0 : 1;
      }
    }

    bool Channel::isOpen() {
      return this->shm_rbuf_in_->isOpen() || this->shm_rbuf_out_->isOpen();
    }
}
