#pragma once

#ifndef GIN_UNIX
#error "Don't include this directly"
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

#include <unordered_map>
#include <vector>

#include "io.h"

namespace gin {

constexpr int MAX_EPOLL_EVENTS = 256;

class UnixEventPort : public EventPort {
public:
	class IFdOwner {
	private:
		UnixEventPort &event_port;
		int file_descriptor;
		int fd_flags;
		uint32_t event_mask;

	public:
		IFdOwner(UnixEventPort &event_port, int file_descriptor, int fd_flags,
				 uint32_t event_mask)
			: event_port{event_port}, file_descriptor{file_descriptor},
			  fd_flags{fd_flags}, event_mask{event_mask} {
			event_port.subscribe(*this, file_descriptor, event_mask);
		}

		~IFdOwner() {
			event_port.unsubscribe(file_descriptor);
			::close(file_descriptor);
		}

		virtual void notify(uint32_t mask) = 0;

		int fd() const { return file_descriptor; }
	};

private:
	int epoll_fd;
	int signal_fd;

	sigset_t signal_fd_set;

	std::unordered_multimap<Signal, Own<ConveyorFeeder<void>>> signal_conveyors;

	std::vector<int> toUnixSignal(Signal signal) const {
		switch (signal) {
		case Signal::Terminate:
		default:
			return {SIGTERM, SIGQUIT, SIGINT};
		}
	}

	Signal fromUnixSignal(int signal) const {
		switch (signal) {
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

class UnixAsyncIoProvider final : public AsyncIoProvider {
private:
	EventLoop event_loop;
	WaitScope wait_scope;

public:
	UnixAsyncIoProvider();

	Own<NetworkAddress> parseAddress(const std::string &) override;

	EventLoop &eventLoop();
	WaitScope &waitScope();
};
} // namespace gin