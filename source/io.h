#pragma once

#include "async.h"
#include "common.h"

#include <string>

namespace gin {

class InputStream {
public:
	virtual ~InputStream() = default;
};

class OutputStream {
public:
	virtual ~OutputStream() = default;
};

class IoStream : public InputStream, public OutputStream {
public:
	virtual ~IoStream() = default;
};

class Server {
public:
	virtual ~Server() = default;

	virtual Own<IoStream> accept() = 0;
};

class NetworkAddress {
public:
	virtual ~NetworkAddress() = default;

	/*
	 * Listen on this address
	 */
	virtual Own<Server> listen() = 0;
	virtual Own<IoStream> connect() = 0;

	virtual std::string toString() = 0;
};

class AsyncIoProvider {
public:
	virtual ~AsyncIoProvider() = default;

	virtual Own<NetworkAddress> parseAddress(const std::string &) = 0;

	virtual Own<InputStream> wrapInputFd(int fd) = 0;
};

struct AsyncIoContext {
	Own<AsyncIoProvider> io;
	EventPort &event_port;
	WaitScope &wait_scope;
};

AsyncIoContext setupAsyncIo();
} // namespace gin