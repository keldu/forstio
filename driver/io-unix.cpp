#include "driver/io-unix.h"

#include <sstream>

namespace gin {
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

namespace {
bool beginsWith(const std::string_view &viewed,
				const std::string_view &begins) {
	return viewed.size() >= begins.size() &&
		   viewed.compare(0, begins.size(), begins) == 0;
}
} // namespace

Own<Server> UnixNetworkAddress::listen() {
	assert(addresses.size() > 0);
	if (addresses.size() == 0) {
		return nullptr;
	}

	int fd = addresses.front().socket(SOCK_STREAM);
	if (fd < 0) {
		return nullptr;
	}

	int val = 1;

	::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

	bool failed = addresses.front().bind(fd);
	if (failed) {
		return nullptr;
	}

	::listen(fd, SOMAXCONN);

	return heap<UnixServer>(event_port, fd, 0);
}

Conveyor<Own<IoStream>> UnixNetworkAddress::connect() {
	assert(addresses.size() > 0);
	if (addresses.size() == 0) {
		return Conveyor<Own<IoStream>>{criticalError("No address found")};
	}

	int fd = addresses.front().socket(SOCK_STREAM);
	if (fd < 0) {
		return Conveyor<Own<IoStream>>{criticalError("Couldn't open socket")};
	}

	Own<UnixIoStream> io_stream =
		heap<UnixIoStream>(event_port, fd, 0, EPOLLIN | EPOLLOUT);

	bool success = false;
	for (auto iter = addresses.begin(); iter != addresses.end(); ++iter) {
		int status = ::connect(fd, iter->getRaw(), iter->getRawLength());
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

std::string UnixNetworkAddress::toString() const {
	try {
		std::ostringstream oss;
		oss << "Address: " << path;
		if (port_hint > 0) {
			oss << "\nPort: " << port_hint;
		}
		return oss.str();
	} catch (std::bad_alloc &) {
		return {};
	}
}
const std::string &UnixNetworkAddress::address() const { return path; }

uint16_t UnixNetworkAddress::port() const { return port_hint; }

UnixNetwork::UnixNetwork(UnixEventPort &event) : event_port{event} {}

Conveyor<Own<NetworkAddress>> UnixNetwork::parseAddress(const std::string &path,
														uint16_t port_hint) {
	std::string_view addr_view{path};
	{
		std::string_view begins_with = "unix:";
		if (beginsWith(addr_view, begins_with)) {
			addr_view.remove_prefix(begins_with.size());
		}
	}

	std::list<SocketAddress> addresses =
		SocketAddress::parse(addr_view, port_hint);

	return Conveyor<Own<NetworkAddress>>{heap<UnixNetworkAddress>(
		event_port, path, port_hint, std::move(addresses))};
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
} // namespace gin
