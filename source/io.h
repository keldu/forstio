#pragma once

#include "common.h"
#include "async.h"

namespace gin {
#ifdef GIN_UNIX
#define Fd int;
#endif

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

	virtual Own<Server> listen() = 0;
	virtual Own<IoStream> connect() = 0;
};

class AsyncIoProvider {
public:
	virtual ~AsyncIoProvider() = default;

	virtual Own<NetworkAddress> parse(const std::string&) = 0;
};

struct AsyncIoContext {
	Own<AsyncIoProvider> io;
	EventLoop& loop;
	WaitScope& wait_scope;
};

AsyncIoContext setupAsyncIo();
}