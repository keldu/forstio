#pragma once

#include "../common.h"
#include "../io.h"

#include <optional>
#include <variant>

namespace saw {
class Tls;

class TlsServer final : public Server {
private:
	Own<Server> internal;

public:
	TlsServer(Own<Server> srv);

	Conveyor<Own<IoStream>> accept() override;
};

class TlsNetwork final : public Network {
private:
	Tls& tls;
	Network &internal;
public:
	TlsNetwork(Tls& tls_, Network &network_);

	Conveyor<Own<NetworkAddress>> resolveAddress(const std::string &addr, uint16_t port = 0) override;
	
	Own<Server> listen(NetworkAddress& address) override;

	Conveyor<Own<IoStream>> connect(NetworkAddress& address) override;

	Own<Datagram> datagram(NetworkAddress& address) override;
};

/**
* Tls context class.
* Provides tls network class which ensures the usage of tls encrypted connections
*/
class Tls {
private:
	class Impl;
	Own<Impl> impl;
public:
	Tls();
	~Tls();

	struct Version {
		struct Tls_1_0{};
		struct Tls_1_1{};
		struct Tls_1_2{};
	};

	struct Options {
	public:
		Version version;
	};

	Network& tlsNetwork();

	Impl &getImpl();
private:
	Options options;
};

std::optional<Own<TlsNetwork>> setupTlsNetwork(Network &network);

} // namespace saw
