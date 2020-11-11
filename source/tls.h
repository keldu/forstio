#pragma once

#include <kelgin/common.h>

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

} // namespace gin
