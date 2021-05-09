#include "driver/io-unix.h"

#include <sstream>

namespace gin {
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

ssize_t UnixIoStream::dataRead(void *buffer, size_t length) {
	return ::read(fd(), buffer, length);
}

ssize_t UnixIoStream::dataWrite(const void *buffer, size_t length) {
	return ::write(fd(), buffer, length);
}
/*
void UnixIoStream::readStep() {
	if (read_ready) {
		read_ready->feed();
	}
	while (!read_tasks.empty()) {
		ReadIoTask &task = read_tasks.front();

		ssize_t n = ::read(fd(), task.buffer, task.max_length);

		if (n <= 0) {
			if (n == 0) {
				if (on_read_disconnect) {
					on_read_disconnect->feed();
				}
				break;
			}
			int error = errno;
			if (error == EAGAIN || error == EWOULDBLOCK) {
				break;
			} else {
				if (read_done) {
					read_done->fail(criticalError("Read failed"));
				}
				read_tasks.pop();
			}
		} else if (static_cast<size_t>(n) >= task.min_length &&
				   static_cast<size_t>(n) <= task.max_length) {
			if (read_done) {
				read_done->feed(static_cast<size_t>(n));
			}
			size_t max_len = task.max_length;
			read_tasks.pop();
		} else {
			task.buffer = reinterpret_cast<uint8_t *>(task.buffer) + n;
			task.min_length -= static_cast<size_t>(n);
			task.max_length -= static_cast<size_t>(n);
		}
	}
}

void UnixIoStream::writeStep() {
	if (write_ready) {
		write_ready->feed();
	}
	while (!write_tasks.empty()) {
		WriteIoTask &task = write_tasks.front();

		ssize_t n = ::write(fd(), task.buffer, task.length);

		if (n < 0) {
			int error = errno;
			if (error == EAGAIN || error == EWOULDBLOCK) {
				break;
			} else {
				if (write_done) {
					write_done->fail(criticalError("Write failed"));
				}
				write_tasks.pop();
			}
		} else if (static_cast<size_t>(n) == task.length) {
			if (write_done) {
				write_done->feed(static_cast<size_t>(task.length));
			}
			write_tasks.pop();
		} else {
			task.buffer = reinterpret_cast<const uint8_t *>(task.buffer) +
						  static_cast<size_t>(n);
			task.length -= static_cast<size_t>(n);
		}
	}
}
*/

UnixIoStream::UnixIoStream(UnixEventPort &event_port, int file_descriptor,
						   int fd_flags, uint32_t event_mask)
	: IFdOwner{event_port, file_descriptor, fd_flags, event_mask | EPOLLRDHUP} {
}

void UnixIoStream::read(void *buffer, size_t min_length, size_t max_length) {
	bool is_ready = read_helper.read_tasks.empty();
	read_helper.read_tasks.push(
		ReadTaskAndStepHelper::ReadIoTask{buffer, min_length, max_length});
	if (is_ready) {
		read_helper.readStep(*this);
	}
}

Conveyor<size_t> UnixIoStream::readDone() {
	auto caf = newConveyorAndFeeder<size_t>();
	read_helper.read_done = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

Conveyor<void> UnixIoStream::readReady() {
	auto caf = newConveyorAndFeeder<void>();
	read_helper.read_ready = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

Conveyor<void> UnixIoStream::onReadDisconnected() {
	auto caf = newConveyorAndFeeder<void>();
	read_helper.on_read_disconnect = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

void UnixIoStream::write(const void *buffer, size_t length) {
	bool is_ready = write_helper.write_tasks.empty();
	write_helper.write_tasks.push(
		WriteTaskAndStepHelper::WriteIoTask{buffer, length});
	if (is_ready) {
		write_helper.writeStep(*this);
	}
}

Conveyor<size_t> UnixIoStream::writeDone() {
	auto caf = newConveyorAndFeeder<size_t>();
	write_helper.write_done = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

Conveyor<void> UnixIoStream::writeReady() {
	auto caf = newConveyorAndFeeder<void>();
	write_helper.write_ready = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

void UnixIoStream::notify(uint32_t mask) {
	if (mask & EPOLLOUT) {
		writeStep();
	}

	if (mask & EPOLLIN) {
		readStep();
	}

	if (mask & EPOLLRDHUP) {
		if (read_helper.on_read_disconnect) {
			read_helper.on_read_disconnect->feed();
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

	addresses.front().bind(fd);
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
				break;
			} else if (error != EINTR) {
				/// @todo Push error message from
				return Conveyor<Own<IoStream>>{
					criticalError("Some error happened.")};
			}
		} else {
			break;
		}
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

UnixAsyncIoProvider::UnixAsyncIoProvider(UnixEventPort &port_ref,
										 Own<EventPort> port)
	: event_port{port_ref}, event_loop{std::move(port)}, unix_network{
															 port_ref} {}

Own<InputStream> UnixAsyncIoProvider::wrapInputFd(int fd) {
	return heap<UnixIoStream>(event_port, fd, 0, EPOLLIN);
}

Network &UnixAsyncIoProvider::network() {
	return static_cast<Network &>(unix_network);
}

EventLoop &UnixAsyncIoProvider::eventLoop() { return event_loop; }

ErrorOr<AsyncIoContext> setupAsyncIo() {
	try {
		Own<UnixEventPort> prt = heap<UnixEventPort>();
		UnixEventPort &prt_ref = *prt;

		Own<UnixAsyncIoProvider> io_provider =
			heap<UnixAsyncIoProvider>(prt_ref, std::move(prt));

		EventLoop &loop_ref = io_provider->eventLoop();

		return {{std::move(io_provider), loop_ref, prt_ref}};
	} catch (std::bad_alloc &) {
		return criticalError("Out of memory");
	}
}
} // namespace gin
