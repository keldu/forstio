#pragma once

#include <kelgin/common.h>
#include <kelgin/io.h>

#include <optional>

namespace gin {
class Tls {
public:
	class Impl;
	Own<Impl> impl;
	
	Tls();
	~Tls();

	class Options {
	public:
	};
};

class TlsIoStream final : public IoStream {
private:
	Own<IoStream> stream;
public:
	TlsIoStream(Own<IoStream> str);

	size_t read(void* buffer, size_t length) override;

	Conveyor<void> readReady() override;

	Conveyor<void> onReadDisconnected() override;

	size_t write(const void* buffer, size_t length) override;

	Conveyor<void> writeReady() override;
};

class TlsServer final : public Server {

};

class TlsNetworkAddress final : public NetworkAddress {
public:
	Own<Server> listen() override;

	Own<IoStream> connect() override;

	std::string toString() override;
};

class TlsNetwork final : public Network {
public:
	TlsNetwork(Network& network);

	Own<NetworkAddress> parseAddress(const std::string& addr, uint16_t port = 0) override;
};

std::optional<Own<TlsNetwork>> setupTlsNetwork(Network& network);

} // namespace gin
