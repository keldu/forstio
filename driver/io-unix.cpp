#include "driver/io-unix.h"

#include <sstream>

namespace saw {
namespace unix {
IFdOwner::IFdOwner(UnixEventPort &event_port, int file_descriptor, int fd_flags,
				   uint32_t event_mask)
	: event_port{event_port}, file_descriptor{file_descriptor},
	  fd_flags{fd_flags}, event_mask{event_mask} {
	event_port.subscribe(*this, file_descriptor, event_mask);
}

IFdOwner::~IFdOwner() {
	if (file_descriptor >= 0) {
		event_port.unsubscribe(file_descriptor);
		::close(file_descriptor);
	}
}

ssize_t unixRead(int fd, void *buffer, size_t length) {
	return ::recv(fd, buffer, length, 0);
}

ssize_t unixWrite(int fd, const void *buffer, size_t length) {
	return ::send(fd, buffer, length, 0);
}

UnixIoStream::UnixIoStream(UnixEventPort &event_port, int file_descriptor,
						   int fd_flags, uint32_t event_mask)
	: IFdOwner{event_port, file_descriptor, fd_flags, event_mask | EPOLLRDHUP} {
}

ErrorOr<size_t> UnixIoStream::read(void *buffer, size_t length) {
	ssize_t read_bytes = unixRead(fd(), buffer, length);
	if (read_bytes > 0) {
		return static_cast<size_t>(read_bytes);
	} else if (read_bytes == 0) {
		return criticalError("Disconnected", Error::Code::Disconnected);
	}

	return recoverableError("Currently busy");
}

Conveyor<void> UnixIoStream::readReady() {
	auto caf = newConveyorAndFeeder<void>();
	read_ready = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

Conveyor<void> UnixIoStream::onReadDisconnected() {
	auto caf = newConveyorAndFeeder<void>();
	on_read_disconnect = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

ErrorOr<size_t> UnixIoStream::write(const void *buffer, size_t length) {
	ssize_t write_bytes = unixWrite(fd(), buffer, length);
	if (write_bytes > 0) {
		return static_cast<size_t>(write_bytes);
	}

	int error = errno;

	if (error == EAGAIN || error == EWOULDBLOCK) {
		return recoverableError("Currently busy");
	}

	return criticalError("Disconnected", Error::Code::Disconnected);
}

Conveyor<void> UnixIoStream::writeReady() {
	auto caf = newConveyorAndFeeder<void>();
	write_ready = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

void UnixIoStream::notify(uint32_t mask) {
	if (mask & EPOLLOUT) {
		if (write_ready) {
			write_ready->feed();
		}
	}

	if (mask & EPOLLIN) {
		if (read_ready) {
			read_ready->feed();
		}
	}

	if (mask & EPOLLRDHUP) {
		if (on_read_disconnect) {
			on_read_disconnect->feed();
		}
	}
}

UnixServer::UnixServer(UnixEventPort &event_port, int file_descriptor,
					   int fd_flags)
	: IFdOwner{event_port, file_descriptor, fd_flags, EPOLLIN} {}

Conveyor<Own<IoStream>> UnixServer::accept() {
	auto caf = newConveyorAndFeeder<Own<IoStream>>();
	accept_feeder = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

void UnixServer::notify(uint32_t mask) {
	if (mask & EPOLLIN) {
		if (accept_feeder) {
			struct ::sockaddr_storage address;
			socklen_t address_length = sizeof(address);

			int accept_fd =
				::accept4(fd(), reinterpret_cast<struct ::sockaddr *>(&address),
						  &address_length, SOCK_NONBLOCK | SOCK_CLOEXEC);
			if (accept_fd < 0) {
				return;
			}
			auto fd_stream = heap<UnixIoStream>(event_port, accept_fd, 0,
												EPOLLIN | EPOLLOUT);
			accept_feeder->feed(std::move(fd_stream));
		}
	}
}

UnixDatagram::UnixDatagram(UnixEventPort &event_port, int file_descriptor,
						   int fd_flags)
	: IFdOwner{event_port, file_descriptor, fd_flags, EPOLLIN | EPOLLOUT} {}

namespace {
ssize_t unixReadMsg(int fd, void *buffer, size_t length) {
	struct ::sockaddr_storage their_addr;
	socklen_t addr_len = sizeof(sockaddr_storage);
	return ::recvfrom(fd, buffer, length, 0,
					  reinterpret_cast<struct ::sockaddr *>(&their_addr),
					  &addr_len);
}

ssize_t unixWriteMsg(int fd, const void *buffer, size_t length,
					 ::sockaddr *dest_addr, socklen_t dest_addr_len) {

	return ::sendto(fd, buffer, length, 0, dest_addr, dest_addr_len);
}
} // namespace

ErrorOr<size_t> UnixDatagram::read(void *buffer, size_t length) {
	ssize_t read_bytes = unixReadMsg(fd(), buffer, length);
	if (read_bytes > 0) {
		return static_cast<size_t>(read_bytes);
	}
	return recoverableError("Currently busy");
}

Conveyor<void> UnixDatagram::readReady() {
	auto caf = newConveyorAndFeeder<void>();
	read_ready = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

ErrorOr<size_t> UnixDatagram::write(const void *buffer, size_t length,
									NetworkAddress &dest) {
	UnixNetworkAddress &unix_dest = static_cast<UnixNetworkAddress &>(dest);
	SocketAddress &sock_addr = unix_dest.unixAddress();
	socklen_t sock_addr_length = sock_addr.getRawLength();
	ssize_t write_bytes = unixWriteMsg(fd(), buffer, length, sock_addr.getRaw(),
									   sock_addr_length);
	if (write_bytes > 0) {
		return static_cast<size_t>(write_bytes);
	}
	return recoverableError("Currently busy");
}

Conveyor<void> UnixDatagram::writeReady() {
	auto caf = newConveyorAndFeeder<void>();
	write_ready = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

void UnixDatagram::notify(uint32_t mask) {
	if (mask & EPOLLOUT) {
		if (write_ready) {
			write_ready->feed();
		}
	}

	if (mask & EPOLLIN) {
		if (read_ready) {
			read_ready->feed();
		}
	}
}

namespace {
bool beginsWith(const std::string_view &viewed,
				const std::string_view &begins) {
	return viewed.size() >= begins.size() &&
		   viewed.compare(0, begins.size(), begins) == 0;
}

std::variant<UnixNetworkAddress, UnixNetworkAddress *>
translateNetworkAddressToUnixNetworkAddress(NetworkAddress &addr) {
	auto addr_variant = addr.representation();
	std::variant<UnixNetworkAddress, UnixNetworkAddress *> os_addr = std::visit(
		[](auto &arg)
			-> std::variant<UnixNetworkAddress, UnixNetworkAddress *> {
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, OsNetworkAddress *>) {
				return static_cast<UnixNetworkAddress *>(arg);
			}

			auto sock_addrs = SocketAddress::resolve(
				std::string_view{arg->address()}, arg->port());

			return UnixNetworkAddress{arg->address(), arg->port(),
									  std::move(sock_addrs)};
		},
		addr_variant);
	return os_addr;
}

UnixNetworkAddress &translateToUnixAddressRef(
	std::variant<UnixNetworkAddress, UnixNetworkAddress *> &addr_variant) {
	return std::visit(
		[](auto &arg) -> UnixNetworkAddress & {
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, UnixNetworkAddress>) {
				return arg;
			} else if constexpr (std::is_same_v<T, UnixNetworkAddress *>) {
				return *arg;
			} else {
				static_assert(true, "Cases exhausted");
			}
		},
		addr_variant);
}

} // namespace

Own<Server> UnixNetwork::listen(NetworkAddress &addr) {
	auto unix_addr_storage = translateNetworkAddressToUnixNetworkAddress(addr);
	UnixNetworkAddress &address = translateToUnixAddressRef(unix_addr_storage);

	assert(address.unixAddressSize() > 0);
	if (address.unixAddressSize() == 0) {
		return nullptr;
	}

	int fd = address.unixAddress(0).socket(SOCK_STREAM);
	if (fd < 0) {
		return nullptr;
	}

	int val = 1;
	int rc = ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
	if (rc < 0) {
		::close(fd);
		return nullptr;
	}

	bool failed = address.unixAddress(0).bind(fd);
	if (failed) {
		::close(fd);
		return nullptr;
	}

	::listen(fd, SOMAXCONN);

	return heap<UnixServer>(event_port, fd, 0);
}

Conveyor<Own<IoStream>> UnixNetwork::connect(NetworkAddress &addr) {
	auto unix_addr_storage = translateNetworkAddressToUnixNetworkAddress(addr);
	UnixNetworkAddress &address = translateToUnixAddressRef(unix_addr_storage);

	assert(address.unixAddressSize() > 0);
	if (address.unixAddressSize() == 0) {
		return Conveyor<Own<IoStream>>{criticalError("No address found")};
	}

	int fd = address.unixAddress(0).socket(SOCK_STREAM);
	if (fd < 0) {
		return Conveyor<Own<IoStream>>{criticalError("Couldn't open socket")};
	}

	Own<UnixIoStream> io_stream =
		heap<UnixIoStream>(event_port, fd, 0, EPOLLIN | EPOLLOUT);

	bool success = false;
	for (size_t i = 0; i < address.unixAddressSize(); ++i) {
		SocketAddress &addr_iter = address.unixAddress(i);
		int status =
			::connect(fd, addr_iter.getRaw(), addr_iter.getRawLength());
		if (status < 0) {
			int error = errno;
			/*
			 * It's not connected yet...
			 * But edge triggered epolling means that it'll
			 * be ready when the signal is triggered
			 */

			/// @todo Add limit node when implemented
			if (error == EINPROGRESS) {
				/*
				Conveyor<void> write_ready = io_stream->writeReady();
				return write_ready.then(
					[ios{std::move(io_stream)}]() mutable {
						ios->write_ready = nullptr;
						return std::move(ios);
					});
				*/
				success = true;
				break;
			} else if (error != EINTR) {
				/// @todo Push error message from
				return Conveyor<Own<IoStream>>{
					criticalError("Couldn't connect")};
			}
		} else {
			success = true;
			break;
		}
	}

	if (!success) {
		return criticalError("Couldn't connect");
	}

	return Conveyor<Own<IoStream>>{std::move(io_stream)};
}

Own<Datagram> UnixNetwork::datagram(NetworkAddress &addr) {
	auto unix_addr_storage = translateNetworkAddressToUnixNetworkAddress(addr);
	UnixNetworkAddress &address = translateToUnixAddressRef(unix_addr_storage);

	SAW_ASSERT(address.unixAddressSize() > 0) { return nullptr; }

	int fd = address.unixAddress(0).socket(SOCK_DGRAM);

	int optval = 1;
	int rc =
		::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (rc < 0) {
		::close(fd);
		return nullptr;
	}

	bool failed = address.unixAddress(0).bind(fd);
	if (failed) {
		::close(fd);
		return nullptr;
	}
	/// @todo
	return heap<UnixDatagram>(event_port, fd, 0);
}

const std::string &UnixNetworkAddress::address() const { return path; }

uint16_t UnixNetworkAddress::port() const { return port_hint; }

SocketAddress &UnixNetworkAddress::unixAddress(size_t i) {
	assert(i < addresses.size());
	/// @todo change from list to vector?
	return addresses.at(i);
}

size_t UnixNetworkAddress::unixAddressSize() const { return addresses.size(); }

UnixNetwork::UnixNetwork(UnixEventPort &event) : event_port{event} {}

Conveyor<Own<NetworkAddress>> UnixNetwork::resolveAddress(const std::string &path,uint16_t port_hint) {
	std::string_view addr_view{path};
	{
		std::string_view begins_with = "unix:";
		if (beginsWith(addr_view, begins_with)) {
			addr_view.remove_prefix(begins_with.size());
		}
	}

	std::vector<SocketAddress> addresses =
		SocketAddress::resolve(addr_view, port_hint);

	return Conveyor<Own<NetworkAddress>>{
		heap<UnixNetworkAddress>(path, port_hint, std::move(addresses))};
}

UnixIoProvider::UnixIoProvider(UnixEventPort &port_ref, Own<EventPort> port)
	: event_port{port_ref}, event_loop{std::move(port)}, unix_network{
		port_ref} {}

Own<InputStream> UnixIoProvider::wrapInputFd(int fd) {
	return heap<UnixIoStream>(event_port, fd, 0, EPOLLIN);
}

Network &UnixIoProvider::network() {
	return static_cast<Network &>(unix_network);
}

EventLoop &UnixIoProvider::eventLoop() { return event_loop; }

} // namespace unix

ErrorOr<AsyncIoContext> setupAsyncIo() {
	using namespace unix;
	try {
		Own<UnixEventPort> prt = heap<UnixEventPort>();
		UnixEventPort &prt_ref = *prt;

		Own<UnixIoProvider> io_provider =
			heap<UnixIoProvider>(prt_ref, std::move(prt));

		EventLoop &loop_ref = io_provider->eventLoop();

		return {{std::move(io_provider), loop_ref, prt_ref}};
	} catch (std::bad_alloc &) {
		return criticalError("Out of memory");
	}
}
} // namespace saw
