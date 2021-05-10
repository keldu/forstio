#include "tls.h"

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "io_helpers.h"

namespace gin {
class TlsContext::Impl {
private:
	gnutls_certificate_credentials_t xcred;

public:
	Impl() {
		gnutls_global_init();
		gnutls_certificate_allocate_credentials(&xcred);
		gnutls_certificate_set_x509_system_trust(xcred);
	}

	~Impl() {
		gnutls_certificate_free_credentials(xcred);
		gnutls_global_deinit();
	}
};

TlsContext::TlsContext() : impl{heap<TlsContext::Impl>()} {}
TlsContext::~TlsContext() {}

class TlsIoStream final : public IoStream, public DataReaderAndWriter {
public:
};

class TlsNetworkAddress : public NetworkAddress {
public:
	Own<Server> listen() override { return nullptr; }
	Conveyor<Own<IoStream>> connect() override { return {nullptr, nullptr}; }
	std::string toString() const override { return {}; }
};

Conveyor<Own<NetworkAddress>> TlsNetwork::parseAddress(const std::string &addr,
													   uint16_t port_hint) {
	return {nullptr, nullptr};
}
} // namespace gin
