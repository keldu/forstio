#pragma once

#include "async.h"
#include "common.h"
#include "io_helpers.h"

#include <string>
#include <variant>

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

class AsyncIoStream final : public AsyncInputStream, public AsyncOutputStream {
private:
	Own<IoStream> stream;

	SinkConveyor read_ready;
	SinkConveyor write_ready;
	SinkConveyor read_disconnected;

	ReadTaskAndStepHelper read_stepper;
	WriteTaskAndStepHelper write_stepper;

public:
	AsyncIoStream(Own<IoStream> str);

	SAW_FORBID_COPY(AsyncIoStream);
	SAW_FORBID_MOVE(AsyncIoStream);

	void read(void *buffer, size_t length, size_t max_length) override;

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

class NetworkAddress;
/**
 * Datagram class. Bound to a local address it is able to receive inbound
 * datagram messages and send them as well as long as an address is provided as
 * well
 */
class Datagram {
public:
	virtual ~Datagram() = default;

	virtual ErrorOr<size_t> read(void *buffer, size_t length) = 0;
	virtual Conveyor<void> readReady() = 0;

	virtual ErrorOr<size_t> write(const void *buffer, size_t length,
								  NetworkAddress &dest) = 0;
	virtual Conveyor<void> writeReady() = 0;
};

class OsNetworkAddress;
class StringNetworkAddress;

class NetworkAddress {
public:
	using ChildVariant =
		std::variant<OsNetworkAddress *, StringNetworkAddress *>;

	virtual ~NetworkAddress() = default;

	virtual NetworkAddress::ChildVariant representation() = 0;

	virtual const std::string &address() const = 0;
	virtual uint16_t port() const = 0;
};

class OsNetworkAddress : public NetworkAddress {
public:
	virtual ~OsNetworkAddress() = default;

	NetworkAddress::ChildVariant representation() override { return this; }
};

class StringNetworkAddress final : public NetworkAddress {
private:
	std::string address_value;
	uint16_t port_value;

public:
	StringNetworkAddress(const std::string &address, uint16_t port);

	const std::string &address() const override;
	uint16_t port() const override;

	NetworkAddress::ChildVariant representation() override { return this; }
};

class Network {
public:
	virtual ~Network() = default;

	/**
	 * Parse the provided string and uint16 to the preferred storage method
	 */
	virtual Conveyor<Own<NetworkAddress>>
	resolveAddress(const std::string &addr, uint16_t port_hint = 0) = 0;

	/**
	 * Set up a listener on this address
	 */
	virtual Own<Server> listen(NetworkAddress &bind_addr) = 0;

	/**
	 * Connect to a remote address
	 */
	virtual Conveyor<Own<IoStream>> connect(NetworkAddress &address) = 0;

	/**
	 * Bind a datagram socket at this address.
	 */
	virtual Own<Datagram> datagram(NetworkAddress &address) = 0;
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
