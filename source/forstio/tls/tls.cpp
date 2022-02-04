#include "tls.h"

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "io_helpers.h"

#include <cassert>

#include <iostream>

namespace saw {

class Tls::Impl {
public:
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

static ssize_t forst_tls_push_func(gnutls_transport_ptr_t p, const void *data,
						 size_t size);
static ssize_t forst_tls_pull_func(gnutls_transport_ptr_t p, void *data, size_t size);

Tls::Tls() : impl{heap<Tls::Impl>()} {}

Tls::~Tls() {}

Tls::Impl &Tls::getImpl() { return *impl; }

class TlsIoStream final : public IoStream {
private:
	Own<IoStream> internal;
	gnutls_session_t session_handle;

public:
	TlsIoStream(Own<IoStream> internal_) : internal{std::move(internal_)} {}

	~TlsIoStream() { gnutls_bye(session_handle, GNUTLS_SHUT_RDWR); }

	ErrorOr<size_t> read(void *buffer, size_t length) override {
		ssize_t size = gnutls_record_recv(session_handle, buffer, length);
		if (size < 0) {
			if(gnutls_error_is_fatal(size) == 0){
				return recoverableError([size](){return std::string{"Read recoverable Error "}+std::string{gnutls_strerror(size)};}, "Error read r");
			}else{
				return criticalError([size](){return std::string{"Read critical Error "}+std::string{gnutls_strerror(size)};}, "Error read c");
			}
		}else if(size == 0){
			return criticalError("Disconnected");
		}

		return static_cast<size_t>(length);
	}

	Conveyor<void> readReady() override { return internal->readReady(); }

	Conveyor<void> onReadDisconnected() override {
		return internal->onReadDisconnected();
	}

	ErrorOr<size_t> write(const void *buffer, size_t length) override {
		ssize_t size = gnutls_record_send(session_handle, buffer, length);
		if(size < 0){
			if(gnutls_error_is_fatal(size) == 0){
				return recoverableError([size](){return std::string{"Write recoverable Error "}+std::string{gnutls_strerror(size)} + " " + std::to_string(size);}, "Error write r");
			}else{
				return criticalError([size](){return std::string{"Write critical Error "}+std::string{gnutls_strerror(size)} + " " + std::to_string(size);}, "Error write c");
			}
		}

		return static_cast<size_t>(size);
	}

	Conveyor<void> writeReady() override { return internal->writeReady(); }

	gnutls_session_t &session() { return session_handle; }
};

TlsServer::TlsServer(Own<Server> srv) : internal{std::move(srv)} {}

Conveyor<Own<IoStream>> TlsServer::accept() {
	SAW_ASSERT(internal) { return Conveyor<Own<IoStream>>{nullptr, nullptr}; }
	return internal->accept().then([](Own<IoStream> stream) -> Own<IoStream> {
		/// @todo handshake


		return heap<TlsIoStream>(std::move(stream));
	});
}

namespace {
/*
* Small helper for setting up the nonblocking connection handshake
*/
struct TlsClientStreamHelper {
public:
	Own<ConveyorFeeder<Own<IoStream>>> feeder;
	SinkConveyor connection_sink;
	SinkConveyor stream_reader;
	SinkConveyor stream_writer;

	Own<TlsIoStream> stream = nullptr;
public:
	TlsClientStreamHelper(Own<ConveyorFeeder<Own<IoStream>>> f):
		feeder{std::move(f)}
	{}

	void setupTurn(){
		SAW_ASSERT(stream){
			return;
		}

		stream_reader = stream->readReady().then([this](){
			turn();
		}).sink();

		stream_writer = stream->writeReady().then([this](){
			turn();
		}).sink();
	}

	void turn(){
		if(stream){
			// Guarantee that the receiving end is already setup
			SAW_ASSERT(feeder){
				return;
			}

			auto &session = stream->session();

			int ret;
			do {
				ret = gnutls_handshake(session);
			} while ( (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED) && gnutls_error_is_fatal(ret) == 0);

			if(gnutls_error_is_fatal(ret)){
				feeder->fail(criticalError("Couldn't create Tls connection"));
				stream = nullptr;
			}else if(ret == GNUTLS_E_SUCCESS){
				feeder->feed(std::move(stream));
			}
		}
	}
};
}

Own<Server> TlsNetwork::listen(NetworkAddress& address) {
	return heap<TlsServer>(internal.listen(address));
}

Conveyor<Own<IoStream>> TlsNetwork::connect(NetworkAddress& address) {
	// Helper setups
	auto caf = newConveyorAndFeeder<Own<IoStream>>();
	Own<TlsClientStreamHelper> helper = heap<TlsClientStreamHelper>(std::move(caf.feeder));
	TlsClientStreamHelper* hlp_ptr = helper.get();
	
	// Conveyor entangled structure
	auto prim_conv = internal.connect(address).then([this, hlp_ptr, addr = address.address()](
										Own<IoStream> stream) -> ErrorOr<void> {
		IoStream* inner_stream = stream.get();
		auto tls_stream = heap<TlsIoStream>(std::move(stream));

		auto &session = tls_stream->session();

		gnutls_init(&session, GNUTLS_CLIENT);

		gnutls_server_name_set(session, GNUTLS_NAME_DNS, addr.c_str(),
							   addr.size());

		gnutls_set_default_priority(session);
		gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
							   tls.getImpl().xcred);
		gnutls_session_set_verify_cert(session, addr.c_str(), 0);

		gnutls_transport_set_ptr(session, reinterpret_cast<gnutls_transport_ptr_t>(inner_stream));
		gnutls_transport_set_push_function(session, forst_tls_push_func);
		gnutls_transport_set_pull_function(session, forst_tls_pull_func);

		// gnutls_handshake_set_timeout(session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);

		hlp_ptr->stream = std::move(tls_stream);
		hlp_ptr->setupTurn();
		hlp_ptr->turn();

		return Void{};
	});

	helper->connection_sink = prim_conv.sink();

	return caf.conveyor.attach(std::move(helper));
}

Own<Datagram> TlsNetwork::datagram(NetworkAddress& address){
	///@unimplemented
	return nullptr;
}

static ssize_t forst_tls_push_func(gnutls_transport_ptr_t p, const void *data,
						 size_t size) {
	IoStream *stream = reinterpret_cast<IoStream *>(p);
	if (!stream) {
		return -1;
	}

	ErrorOr<size_t> length = stream->write(data, size);
	if (length.isError() || !length.isValue()) {
		return -1;
	}

	return static_cast<ssize_t>(length.value());
}

static ssize_t forst_tls_pull_func(gnutls_transport_ptr_t p, void *data, size_t size) {
	IoStream *stream = reinterpret_cast<IoStream *>(p);
	if (!stream) {
		return -1;
	}

	ErrorOr<size_t> length = stream->read(data, size);
	if (length.isError() || !length.isValue()) {
		return -1;
	}

	return static_cast<ssize_t>(length.value());
}

TlsNetwork::TlsNetwork(Network &network) : internal{network} {}

Conveyor<Own<NetworkAddress>> TlsNetwork::parseAddress(const std::string &addr,
													   uint16_t port) {
	/// @todo tls server name needed. Check validity. Won't matter later on, because gnutls should fail anyway. But
	/// it's better to find the error source sooner rather than later
	return internal.parseAddress(addr, port);
}

ErrorOr<SocketPair> TlsNetwork::socketPair(){
	return criticalError("Unimplemented");
}

std::optional<Own<TlsNetwork>> setupTlsNetwork(Network &network) {
	return std::nullopt;
}
} // namespace saw
