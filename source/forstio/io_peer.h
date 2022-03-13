#pragma once

#include "async.h"
#include "message.h"
#include "io.h"

namespace saw {

template <typename Codec, typename Incoming, typename Outgoing, typename InContainer = MessageContainer<Incoming>, typename OutContainer = MessageContainer<Outgoing>, typename BufferT = RingBuffer>
class StreamingIoPeer {
private:
	Own<ConveyorFeeder<HeapMessageRoot<Incoming, InContainer>>> incoming_feeder = nullptr;

	Own<AsyncIoStream> io_stream;

	Codec codec;

	BufferT in_buffer;
	BufferT out_buffer;

	SinkConveyor sink_read;
	SinkConveyor sink_write;

public:
	StreamingIoPeer(Own<ConveyorFeeder<HeapMessageRoot<Incoming, InContainer>>> feed, Own<AsyncIoStream> stream, Codec codec, BufferT in, BufferT out);
	StreamingIoPeer(Own<ConveyorFeeder<HeapMessageRoot<Incoming, InContainer>>> feed, Own<AsyncIoStream> stream); 

	void send(HeapMessageRoot<Outgoing, OutContainer> builder);

	Conveyor<void> onReadDisconnected();
};

/**
 * Setup new streaming io peer with the provided network protocols.
 * This is a convenience wrapper intended for a faster setup of 
 */
template <typename Codec, typename Incoming, typename Outgoing, typename InContainer = MessageContainer<Incoming>, typename OutContainer = MessageContainer<Outgoing>, typename BufferT = RingBuffer>
std::pair<StreamingIoPeer<Codec, Incoming, Outgoing, InContainer, OutContainer, BufferT>, Conveyor<HeapMessageRoot<Incoming, InContainer>>> newStreamingIoPeer(Own<AsyncIoStream> stream);

} // namespace saw

#include "io_peer.tmpl.h"
