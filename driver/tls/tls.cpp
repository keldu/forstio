#include "tls.h"

namespace gin {
TlsContext::TlsContext() {
	gnutls_global_init();
	gnutls_certificate_allocate_credentials(&xcred);
	gnutls_certificate_set_x509_system_trust(xcred);
}

TlsContext::~TlsContext() {
	gnutls_certificate_free_credentials(xcred);
	gnutls_global_deinit();
}

TlsNetwork::TlsNetwork(Network &net) : network{net} {}

std::optional<Own<Network>> setupTlsNetwork(Network &network) {

	return std::nullopt;
}
} // namespace gin