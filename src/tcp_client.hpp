
#ifndef TCPCLIENT_H_
#define TCPCLIENT_H_

#include <string>
#include <vector>
#include <zmq.hpp>
#include "format.hpp"
#include "request.hpp"

namespace janosh {
using std::string;
using std::vector;

class TcpClient {
  zmq::context_t context_;
  zmq::socket_t sock_;

public:
	TcpClient();
	void connect(string url);
	int run(Request& req, std::ostream& out);
	void close(bool commit);
};

} /* namespace janosh */

#endif /* CHEESYCLIENT_H_ */
