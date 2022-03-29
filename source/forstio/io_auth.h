#pragma once

#include "io.h"

namespace saw {
class Peer {
public:
	Peer(const std::string &ident);
	Peer(std::string &&ident);

	const std::string &identity() const;

private:
	std::string identity_value;
};

class AuthenticatedIoStream {
public:
	// This is the easiest way to implement Authenticated streams.
	// This is a simple pair of the stream and the peer.

	Own<IoStream> stream;
	Maybe<Own<Peer>> peer;
};

class AuthenticatedServer {
public:
	virtual ~AuthenticatedServer() = default;

	virtual Conveyor<AuthenticatedIoStream> accept() = 0;
};

/**
 * Authenticated Network class which provides a peer identity when connecting
 */
class AuthenticatedNetwork {
public:
	virtual ~AuthenticatedNetwork() = default;

	/**
	 * Connects to the provided address.
	 * Returns as soon as it is authenticated or fails
	 */
	virtual Conveyor<AuthenticatedIoStream>
	connect(NetworkAddress &address) = 0;

	/**
	 * Creates a server listening for connections
	 */
	virtual Own<AuthenticatedServer> listen() = 0;
};


} // namespace saw
