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

class TlsNetworkAddress final : public NetworkAddress {
private:
	Own<NetworkAddress> internal;
	std::string host_name;
	Tls &tls;

public:
	TlsNetworkAddress(Own<NetworkAddress> net_addr, const std::string& host_name_, Tls &tls_);

	Own<Server> listen() override;

	Conveyor<Own<IoStream>> connect() override;

	Own<Datagram> datagram() override;

	std::string toString() const override;

	const std::string &address() const override;
	uint16_t port() const override;
};

class TlsNetwork final : public Network {
private:
	Tls tls;
	Network &internal;

public:
	TlsNetwork(Network &network);

	Conveyor<Own<NetworkAddress>> parseAddress(const std::string &addr,
											   uint16_t port = 0) override;
};

std::optional<Own<TlsNetwork>> setupTlsNetwork(Network &network);

} // namespace saw
