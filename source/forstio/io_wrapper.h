#pragma once

#include "async.h"
#include "io.h"

namespace saw {

template <typename Codec, typename Incoming, typename Outgoing>
class StreamingIoPeer {
private:
	Codec codec;

	Own<AsyncIoStream> io_stream;

	Own<ConveyorFeeder<Incoming>> incoming_feeder = nullptr;

public:
	StreamingIoPeer(Own<AsyncIoStream> stream);

	void send(Outgoing outgoing, Own<MessageBuilder> builder);

	Conveyor<Incoming> startReadPump();
};

} // namespace saw