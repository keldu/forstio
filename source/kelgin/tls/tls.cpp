#include "tls.h"

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "io_helpers.h"

namespace gin {

class Tls::Impl {
public:
	Impl(){
		gnutls_global_init();
		gnutls_certificate_allocate_credentials(&xcred);
		gnutls_certificate_set_x509_system_trust(xcred);
	}

	~Impl(){
		gnutls_certificate_free_credentials(xcred);
		gnutls_global_deinit();
	}
};

Tls::Tls():
	impl{heap<Tls::Impl>()}
{}

Tls::~Tls(){}

class TlsNetworkImpl final : public TlsNetwork {
public:
};
} // namespace gin
