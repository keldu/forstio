#pragma once

#include "async.h"
#include "message.h"
#include "io.h"

namespace saw {

template <typename Codec, typename Incoming, typename Outgoing, class InContainer = MessageContainer<Incoming>, class OutContainer = MessageContainer<Outgoing>>
class StreamingIoPeer {
private:
	Codec codec;

	Own<AsyncIoStream> io_stream;

	Own<ConveyorFeeder<HeapMessageRoot<Incoming, InContainer>>> incoming_feeder = nullptr;
public:
	StreamingIoPeer(Own<AsyncIoStream> stream);

	void send(HeapMessageRoot<Outgoing, OutContainer> builder);

	Conveyor<HeapMessageRootIncoming> startReadPump();
};



} // namespace saw
