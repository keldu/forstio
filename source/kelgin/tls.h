#pragma once

#include <kelgin/common.h>
#include <kelgin/io.h>

namespace gin {
class TlsContext {
public:
	struct Options {};

private:
	/*
	 * Pimpl pattern to hide GnuTls includes
	 */
	class Impl;
	Own<Impl> impl;

public:
	TlsContext();
	~TlsContext();
};

class TlsNetwork final : public Network {
private:
	TlsContext context;

public:
	Conveyor<Own<NetworkAddress>> parseAddress(const std::string &,
											   uint16_t port_hint = 0) override;
};

} // namespace gin
