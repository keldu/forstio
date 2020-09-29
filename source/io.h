#pragma once

#include "async.h"
#include "common.h"

#include <string>

namespace gin {

class InputStream {
public:
	virtual ~InputStream() = default;

	virtual void read(void *buffer, size_t min_length, size_t max_length) = 0;
	virtual Conveyor<size_t> readDone() = 0;
	virtual Conveyor<void> readReady() = 0;

	virtual Conveyor<void> onReadDisconnected() = 0;
};

class OutputStream {
public:
	virtual ~OutputStream() = default;

	virtual void write(const void *buffer, size_t length) = 0;
	virtual Conveyor<size_t> writeDone() = 0;
	virtual Conveyor<void> writeReady() = 0;
};

class IoStream : public InputStream, public OutputStream {
public:
	virtual ~IoStream() = default;
};

class Server {
public:
	virtual ~Server() = default;

	virtual Conveyor<Own<IoStream>> accept() = 0;
};

class NetworkAddress {
public:
	virtual ~NetworkAddress() = default;

	/*
	 * Listen on this address
	 */
	virtual Own<Server> listen() = 0;
	virtual Conveyor<Own<IoStream>> connect() = 0;

	virtual std::string toString() const = 0;
};

class AsyncIoProvider {
public:
	virtual ~AsyncIoProvider() = default;

	virtual Own<NetworkAddress> parseAddress(const std::string &,
											 uint16_t port_hint = 0) = 0;

	virtual Own<InputStream> wrapInputFd(int fd) = 0;
};

struct AsyncIoContext {
	Own<AsyncIoProvider> io;
	EventPort &event_port;
	WaitScope &wait_scope;
};

AsyncIoContext setupAsyncIo();
} // namespace gin