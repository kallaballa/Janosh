#ifndef CHANNEL_HPP_
#define CHANNEL_HPP_

#include <vector>
#include <string>
#include <iostream>
#include "ipc.hpp"

namespace janosh {
  class Channel {
  private:
    ringbuffer_connection* shm_rbuf_in_;
    ringbuffer_connection* shm_rbuf_out_;
    ringbuffer_connection::istream* in_;
    ringbuffer_connection::ostream* out_;
    bool daemon_;
  public:
    Channel();
    ~Channel();
    void accept();
    void connect();
    bool isOpen();
    std::istream& in();
    std::ostream& out();
    void writeln(const string& s);
    void flush(bool success);
    bool receive(std::vector<std::string>& v);
    bool receive(std::ostream& os);

  };
}
#endif /* CHANNEL_HPP_ */
