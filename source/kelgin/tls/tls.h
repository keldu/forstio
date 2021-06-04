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

class TlsNetworkAddress final : public NetworkAddress {
public:
};

class TlsNetwork final : public Network {
public:
	TlsNetwork(Network& network);
};

std::optional<Own<TlsNetwork>> setupTlsNetwork(Network& network);

} // namespace gin
