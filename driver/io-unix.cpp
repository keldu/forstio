#include "driver/io-unix.h"

namespace gin {
Own<Server> UnixNetworkAddress::listen(){
	return nullptr;
}

Own<IoStream> UnixNetworkAddress::connect(){
	return nullptr;
}

std::string UnixNetworkAddress::toString() {
	return "";
}

UnixAsyncIoProvider::UnixAsyncIoProvider()
	: event_loop{heap<UnixEventPort>()}, wait_scope{event_loop} {}

Own<NetworkAddress> UnixAsyncIoProvider::parseAddress(const std::string &path) {
	return nullptr;
}

Own<InputStream> UnixAsyncIoProvider::wrapInputFd(int fd) {
	return nullptr;
}

EventLoop &UnixAsyncIoProvider::eventLoop() { return event_loop; }

WaitScope &UnixAsyncIoProvider::waitScope() { return wait_scope; }

AsyncIoContext setupAsyncIo() {
	Own<UnixAsyncIoProvider> io_provider = heap<UnixAsyncIoProvider>();

	EventLoop &event_loop = io_provider->eventLoop();
	EventPort *event_port = event_loop.eventPort();
	WaitScope &wait_scope = io_provider->waitScope();
	return {std::move(io_provider), *event_port, wait_scope};
}
} // namespace gin