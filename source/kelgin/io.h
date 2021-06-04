#pragma once

#include "async.h"
#include "common.h"

#include <string>

namespace gin {
/*
 * Input stream
 */
class InputStream {
public:
	virtual ~InputStream() = default;

	virtual ssize_t read(void *buffer, size_t length) = 0;

	virtual Conveyor<void> readReady() = 0;

	virtual Conveyor<void> onReadDisconnected() = 0;
};

/*
 * Output stream
 */
class OutputStream {
public:
	virtual ~OutputStream() = default;

	virtual ssize_t write(const void *buffer, size_t length) = 0;

	virtual Conveyor<void> writeReady() = 0;
};

/*
 * Io stream
 */
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

class Network {
public:
	virtual ~Network() = default;

	virtual Conveyor<Own<NetworkAddress>>
	parseAddress(const std::string &addr, uint16_t port_hint = 0) = 0;
};

class IoProvider {
public:
	virtual ~IoProvider() = default;

	virtual Own<InputStream> wrapInputFd(int fd) = 0;

	virtual Network &network() = 0;
};

struct AsyncIoContext {
	Own<IoProvider> io;
	EventLoop &event_loop;
	EventPort &event_port;
};

ErrorOr<AsyncIoContext> setupAsyncIo();
} // namespace gin
