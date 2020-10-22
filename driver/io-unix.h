#pragma once

#ifndef GIN_UNIX
#error "Don't include this"
#endif

#include <csignal>
#include <sys/signalfd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <cassert>
#include <cstring>

#include <errno.h>
#include <unistd.h>

#include <queue>
#include <unordered_map>
#include <vector>

#include "io.h"

namespace gin {
constexpr int MAX_EPOLL_EVENTS = 256;

class UnixEventPort;
class IFdOwner {
protected:
	UnixEventPort &event_port;

private:
	int file_descriptor;
	int fd_flags;
	uint32_t event_mask;

public:
	IFdOwner(UnixEventPort &event_port, int file_descriptor, int fd_flags,
			 uint32_t event_mask);

	virtual ~IFdOwner();

	virtual void notify(uint32_t mask) = 0;

	int fd() const { return file_descriptor; }
};

class UnixEventPort : public EventPort {
private:
	int epoll_fd;
	int signal_fd;

	sigset_t signal_fd_set;

	std::unordered_multimap<Signal, Own<ConveyorFeeder<void>>> signal_conveyors;

	std::vector<int> toUnixSignal(Signal signal) const {
		switch (signal) {
		case Signal::User1:
			return {SIGUSR1};
		case Signal::Terminate:
		default:
			return {SIGTERM, SIGQUIT, SIGINT};
		}
	}

	Signal fromUnixSignal(int signal) const {
		switch (signal) {
		case SIGUSR1:
			return Signal::User1;
		case SIGTERM:
		case SIGINT:
		case SIGQUIT:
		default:
			return Signal::Terminate;
		}
	}

	void notifySignalListener(int sig) {
		Signal signal = fromUnixSignal(sig);

		auto equal_range = signal_conveyors.equal_range(signal);
		for (auto iter = equal_range.first; iter != equal_range.second;
			 ++iter) {

			if (iter->second) {
				if (iter->second->space() > 0) {
					iter->second->feed();
				}
			}
		}
	}

	bool pollImpl(int time) {
		epoll_event events[MAX_EPOLL_EVENTS];
		int nfds = 0;
		do {
			nfds = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, time);

			if (nfds < 0) {
				/// @todo error_handling
				assert(false);
				return false;
			}

			for (int i = 0; i < nfds; ++i) {
				if (events[i].data.u64 == 0) {
					for (;;) {
						struct ::signalfd_siginfo siginfo;
						ssize_t n =
							::read(signal_fd, &siginfo, sizeof(siginfo));
						if (n < 0) {
							break;
						}
						assert(n == sizeof(siginfo));

						notifySignalListener(siginfo.ssi_signo);
					}
				} else {
					IFdOwner *owner =
						reinterpret_cast<IFdOwner *>(events[i].data.ptr);
					if (owner) {
						owner->notify(events[i].events);
					}
				}
			}
		} while (nfds == MAX_EPOLL_EVENTS);

		return true;
	}

public:
	UnixEventPort() : epoll_fd{-1}, signal_fd{-1} {
		::signal(SIGPIPE, SIG_IGN);

		epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
		if (epoll_fd < 0) {
			return;
		}

		::sigemptyset(&signal_fd_set);
		signal_fd = ::signalfd(-1, &signal_fd_set, SFD_NONBLOCK | SFD_CLOEXEC);
		if (signal_fd < 0) {
			return;
		}

		struct epoll_event event;
		memset(&event, 0, sizeof(event));
		event.events = EPOLLIN;
		event.data.u64 = 0;
		::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &event);
	}

	~UnixEventPort() {
		::close(epoll_fd);
		::close(signal_fd);
	}

	Conveyor<void> onSignal(Signal signal) override {
		auto caf = newConveyorAndFeeder<void>();

		signal_conveyors.insert(std::make_pair(signal, std::move(caf.feeder)));

		std::vector<int> sig = toUnixSignal(signal);

		for (auto iter = sig.begin(); iter != sig.end(); ++iter) {
			::sigaddset(&signal_fd_set, *iter);
		}
		::sigprocmask(SIG_BLOCK, &signal_fd_set, nullptr);
		::signalfd(signal_fd, &signal_fd_set, SFD_NONBLOCK | SFD_CLOEXEC);

		auto node_and_storage =
			Conveyor<void>::fromConveyor(std::move(caf.conveyor));
		return Conveyor<void>::toConveyor(std::move(node_and_storage.first),
										  node_and_storage.second);
	}

	void poll() override { pollImpl(0); }

	void wait() override { pollImpl(-1); }

	void wait(const std::chrono::steady_clock::duration &duration) override {
		pollImpl(std::chrono::duration_cast<std::chrono::milliseconds>(duration)
					 .count());
	}
	void
	wait(const std::chrono::steady_clock::time_point &time_point) override {
		auto now = std::chrono::steady_clock::now();
		if (time_point <= now) {
			poll();
		} else {
			pollImpl(std::chrono::duration_cast<std::chrono::milliseconds>(
						 time_point - now)
						 .count());
		}
	}

	void subscribe(IFdOwner &owner, int fd, uint32_t event_mask) {
		if (epoll_fd < 0 || fd < 0) {
			return;
		}
		::epoll_event event;
		memset(&event, 0, sizeof(event));
		event.events = event_mask | EPOLLET;
		event.data.ptr = &owner;

		if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0) {
			/// @todo error_handling
			return;
		}
	}

	void unsubscribe(int fd) {
		if (epoll_fd < 0 || fd < 0) {
			return;
		}
		if (::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
			/// @todo error_handling
			return;
		}
	}
};

class UnixIoStream : public IoStream, public IFdOwner {
private:
	struct WriteIoTask {
		const void *buffer;
		size_t length;
	};
	std::queue<WriteIoTask> write_tasks;
	Own<ConveyorFeeder<size_t>> write_done = nullptr;
	Own<ConveyorFeeder<void>> write_ready = nullptr;

	struct ReadIoTask {
		void *buffer;
		size_t min_length;
		size_t max_length;
	};
	std::queue<ReadIoTask> read_tasks;
	Own<ConveyorFeeder<size_t>> read_done = nullptr;
	Own<ConveyorFeeder<void>> read_ready = nullptr;

	Own<ConveyorFeeder<void>> on_read_disconnect = nullptr;

private:
	void readStep();
	void writeStep();

public:
	UnixIoStream(UnixEventPort &event_port, int file_descriptor, int fd_flags,
				 uint32_t event_mask);

	void read(void *buffer, size_t min_length, size_t max_length) override;
	Conveyor<size_t> readDone() override;
	Conveyor<void> readReady() override;

	Conveyor<void> onReadDisconnected() override;

	void write(const void *buffer, size_t length) override;
	Conveyor<size_t> writeDone() override;
	Conveyor<void> writeReady() override;

	void notify(uint32_t mask) override;
};

class UnixServer : public Server, public IFdOwner {
private:
	Own<ConveyorFeeder<Own<IoStream>>> accept_feeder = nullptr;

public:
	UnixServer(UnixEventPort &event_port, int file_descriptor, int fd_flags);

	Conveyor<Own<IoStream>> accept() override;

	void notify(uint32_t mask) override;
};

class SocketAddress {
private:
	union {
		struct sockaddr generic;
		struct sockaddr_un unix;
		struct sockaddr_in inet;
		struct sockaddr_in6 inet6;
		struct sockaddr_storage storage;
	} address;

	socklen_t address_length;
	bool wildcard;

	SocketAddress() : wildcard{false} {}

public:
	SocketAddress(const void *sockaddr, socklen_t len, bool wildcard)
		: address_length{len}, wildcard{wildcard} {
		assert(len <= sizeof(address));
		memcpy(&address.generic, sockaddr, len);
	}

	int socket(int type) const {
		bool is_stream = type & SOCK_STREAM;

		type |= SOCK_NONBLOCK | SOCK_CLOEXEC;

		int result = ::socket(address.generic.sa_family, type, 0);
		return result;
	}

	void bind(int fd) const {
		if (wildcard) {
			int value = 0;
			::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &value, sizeof(value));
		}
		::bind(fd, &address.generic, address_length);
	}

	const struct ::sockaddr *getRaw() const { return &address.generic; }

	socklen_t getRawLength() const { return address_length; }

	static std::list<SocketAddress> parse(std::string_view str,
										  uint16_t port_hint) {
		std::list<SocketAddress> results;

		struct ::addrinfo *head;
		struct ::addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;

		std::string port_string = std::to_string(port_hint);
		bool wildcard = str == "*" || str == "::";
		std::string address_string{str};

		int error = ::getaddrinfo(address_string.c_str(), port_string.c_str(),
								  &hints, &head);

		if (error) {
			return {};
		}

		for (struct ::addrinfo *it = head; it != nullptr; it = it->ai_next) {
			if (it->ai_addrlen > sizeof(SocketAddress::address)) {
				continue;
			}
			results.push_back({it->ai_addr, it->ai_addrlen, wildcard});
		}
		::freeaddrinfo(head);
		return results;
	}
};

class UnixNetworkAddress : public NetworkAddress {
private:
	UnixEventPort &event_port;
	AsyncIoProvider &io_provider;
	const std::string path;
	uint16_t port_hint;
	std::list<SocketAddress> addresses;

public:
	UnixNetworkAddress(UnixEventPort &event_port, AsyncIoProvider &io_provider,
					   const std::string &path, uint16_t port_hint,
					   std::list<SocketAddress> &&addr)
		: event_port{event_port}, io_provider{io_provider}, path{path},
		  port_hint{port_hint}, addresses{std::move(addr)} {}

	Own<Server> listen() override;
	Own<IoStream> connect() override;

	std::string toString() const override;
};

class UnixAsyncIoProvider final : public AsyncIoProvider {
private:
	EventLoop event_loop;
	WaitScope wait_scope;

public:
	UnixAsyncIoProvider();

	Own<NetworkAddress> parseAddress(const std::string &,
									 uint16_t port_hint = 0) override;

	Own<InputStream> wrapInputFd(int fd) override;

	EventLoop &eventLoop();
	WaitScope &waitScope();
};
} // namespace gin