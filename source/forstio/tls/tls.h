#pragma once

#include "../common.h"
#include "../io.h"

#include <optional>
#include <variant>

namespace saw {
class Tls {
private:
	class Impl;
	Own<Impl> impl;

public:
	Tls();
	~Tls();

	class Options {
	public:
	};

	Impl &getImpl();
};

class TlsServer final : public Server {
private:
	Own<Server> internal;

public:
	TlsServer(Own<Server> srv);

	Conveyor<Own<IoStream>> accept() override;
};

class TlsNetwork final : public Network {
private:
	Tls tls;
	Network &internal;

public:
	TlsNetwork(Network &network);

	Conveyor<Own<NetworkAddress>> parseAddress(const std::string &addr, uint16_t port = 0) override;
	
	Own<Server> listen(NetworkAddress& address) override;

	Conveyor<Own<IoStream>> connect(NetworkAddress& address) override;

	Own<Datagram> datagram(NetworkAddress& address) override;
};

std::optional<Own<TlsNetwork>> setupTlsNetwork(Network &network);

} // namespace saw
