#pragma once

#include <kelgin/common.h>
#include <kelgin/io.h>

#include <optional>

namespace gin {
class Tls {
public:
	class Options {
	public:
	};
};

std::optional<Own<Network>> setupTlsNetwork(Network& network);

} // namespace gin
