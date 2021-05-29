#pragma once

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "common.h"

#include <kelgin/tls/tls.h>

namespace gin {
class TlsContext {
public:
	gnutls_certificate_credentials_t xcred;
public:
	TlsContext();
	~TlsContext();

	GIN_FORBID_COPY(TlsContext);
};

class TlsNetwork : public Network {
private:
	Network& network;
	TlsContext context;
public:
	TlsNetwork(Network& net);

	Conveyor<Own<NetworkAddress>>
	parseAddress(const std::string &addr, uint16_t port_hint = 0) override;
};


}