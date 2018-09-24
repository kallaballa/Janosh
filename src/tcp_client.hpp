
#ifndef TCPCLIENT_H_
#define TCPCLIENT_H_

#include <string>
#include <vector>
#include "nn.hpp"
#include "nanomsg/pair.h"
#include "format.hpp"
#include "request.hpp"

namespace janosh {
using std::string;
using std::vector;

class TcpClient {
  nn::socket sock_;

public:
	TcpClient();
	virtual ~TcpClient();
	void connect(std::string host, int port);
	int run(Request& req, std::ostream& out);
	void close();
};

} /* namespace janosh */

#endif /* CHEESYCLIENT_H_ */
