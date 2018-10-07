
#ifndef TCPCLIENT_H_
#define TCPCLIENT_H_

#include "format.hpp"
#include "request.hpp"
#include <string>
#include <vector>
#include <libsocket/unixclientstream.hpp>
#include <libsocket/exception.hpp>


namespace janosh {

namespace ls = libsocket;
using std::string;
using std::vector;

class TcpClient {
  ls::unix_stream_client sock_;
  std::string rcvBuffer_;

public:
	TcpClient();
	virtual ~TcpClient();
	void connect(string url);
	void send(const string& msg);
	void receive(string& msg);
	int run(Request& req, std::ostream& out);
	void close(bool commit);
};

} /* namespace janosh */

#endif /* CHEESYCLIENT_H_ */
