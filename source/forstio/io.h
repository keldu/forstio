#pragma once

#include "async.h"
#include "common.h"
#include "io_helpers.h"

#include <string>

namespace saw {
/*
 * Input stream
 */
class InputStream {
public:
	virtual ~InputStream() = default;

	virtual ErrorOr<size_t> read(void *buffer, size_t length) = 0;

	virtual Conveyor<void> readReady() = 0;

	virtual Conveyor<void> onReadDisconnected() = 0;
};

/*
 * Output stream
 */
class OutputStream {
public:
	virtual ~OutputStream() = default;

	virtual ErrorOr<size_t> write(const void *buffer, size_t length) = 0;

	virtual Conveyor<void> writeReady() = 0;
};

/*
 * Io stream
 */
class IoStream : public InputStream, public OutputStream {
public:
	virtual ~IoStream() = default;
};

class AsyncInputStream {
public:
	virtual ~AsyncInputStream() = default;

	virtual void read(void *buffer, size_t min_length, size_t max_length) = 0;

	virtual Conveyor<size_t> readDone() = 0;
	virtual Conveyor<void> onReadDisconnected() = 0;
};

class AsyncOutputStream {
public:
	virtual ~AsyncOutputStream() = default;

	virtual void write(const void *buffer, size_t length) = 0;

	virtual Conveyor<size_t> writeDone() = 0;
};

class AsyncIoStream : public AsyncInputStream, public AsyncOutputStream {
private:
	Own<IoStream> stream;

	SinkConveyor read_ready;
	SinkConveyor write_ready;
	SinkConveyor read_disconnected;

	ReadTaskAndStepHelper read_stepper;
	WriteTaskAndStepHelper write_stepper;

public:
	AsyncIoStream(Own<IoStream> str);

	void read(void *buffer, size_t min_length, size_t max_length) override;

	Conveyor<size_t> readDone() override;

	Conveyor<void> onReadDisconnected() override;

	void write(const void *buffer, size_t length) override;

	Conveyor<size_t> writeDone() override;
};

class Server {
public:
	virtual ~Server() = default;

	virtual Conveyor<Own<IoStream>> accept() = 0;
};

class SocketPair {
public:
	std::array<Own<IoStream>, 2> stream;
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

	virtual const std::string &address() const = 0;
	virtual uint16_t port() const = 0;
};

class Network {
public:
	virtual ~Network() = default;

	virtual Conveyor<Own<NetworkAddress>>
	parseAddress(const std::string &addr, uint16_t port_hint = 0) = 0;

	/**
	 * Creates an unnamed pair of bidirectional fds
	 */
	virtual ErrorOr<SocketPair> socketPair() = 0;
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
} // namespace saw
