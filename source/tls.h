#pragma once

#include <kelgin/common.h>

namespace gin {
class TlsContext {
private:
	/*
	* Pimpl pattern to hide GnuTls include
	*/
	class Impl;
	Own<Impl> impl;
public:
	TlsContext();
	~TlsContext();
};

class TlsIoProvider {

};

Own<TlsNetworkProvider> tlsUp
}