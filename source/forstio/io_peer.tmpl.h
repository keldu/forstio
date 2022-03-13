namespace saw {

template <typename Codec, typename Incoming, typename Outgoing, typename InContainer, typename OutContainer, typename BufferT>
StreamingIoPeer<Codec, Incoming, Outgoing, InContainer, OutContainer, BufferT>::StreamingIoPeer(
Own<ConveyorFeeder<HeapMessageRoot<Incoming, InContainer>>> feed,
Own<AsyncIoStream> str
):
	StreamingIoPeer{std::move(feed), std::move(str), {}, {}, {}}
{
}

template <typename Codec, typename Incoming, typename Outgoing, typename InContainer, typename OutContainer, typename BufferT>
StreamingIoPeer<Codec, Incoming, Outgoing, InContainer, OutContainer, BufferT>::StreamingIoPeer(
Own<ConveyorFeeder<HeapMessageRoot<Incoming, InContainer>>> feed,
Own<AsyncIoStream> str, Codec codec, BufferT in, BufferT out):
	incoming_feeder{std::move(feed)},
	io_stream{std::move(str)},
	codec{std::move(codec)},
	in_buffer{std::move(in)},
	out_buffer{std::move(out)},
	sink_read{
		io_stream->readDone().then([this](size_t bytes) -> ErrorOr<void> {
			in_buffer.writeAdvance(bytes);

			if(in_buffer.writeSegmentLength() == 0){
				return criticalError("Message too long");
			}

			io_stream->read(&in_buffer.write(), 1, in_buffer.writeSegmentLength());

			while(true){
				auto root = heapMessageRoot<Incoming, InContainer>();
				auto builder = root.build();

				Error error = codec.template decode<Incoming, InContainer>(builder, in_buffer);
				if(error.isCritical()){
					return error;
				}

				if(!error.failed()){
					incoming_feeder->handle(std::move(root));
				}else{
					break;
				}
			}

			return Void{};
		}).sink([this](Error error){
			incoming_feeder->fail(error);

			return error;
		})
	},
	sink_write{
		io_stream->writeDone().then([this](size_bytes) -> ErrorOr<void> {
			out_buffer.readAdvance(bytes);
			if(out_buffer.readCompositeLength() > 0){
				io_stream->write(&out_buffer.read(), out_buffer.readSegmengtLength());
			}

			return Void{};
		}).sink();
	}
{
	io_stream->read(&in_buffer.write(), 1, in_buffer.writeSegmentLength());
}

template <typename Codec, typename Incoming, typename Outgoing, typename InContainer, typename OutContainer, typename BufferT>
void StreamingIoPeer<Codec, Incoming, Outgoing, InContainer, OutContainer, BufferT>::send(HeapMessageRoot<Outgoing, OutContainer> msg){
	bool restart_write = out_buffer.readSegmentLength() == 0;
	
	Error error = codec.template encode<Outgoing, OutContainer>(msg.read(), out_buffer);
	if(error.failed()){
		return error;
	}

	if(restart_write){
		io_stream->write(&out_buffer.read(), out_buffer.readSegmentLength());
	}

	return noError();
}

template <typename Codec, typename Incoming, typename Outgoing, typename InContainer, typename OutContainer, typename BufferT>
Conveyor<void> StreamingIoPeer<Codec, Incoming, Outgoing, InContainer, OutContainer, BufferT>::onReadDisconnected(){
	return io_stream->onReadDisconnected();
}

template <typename Codec, typename Incoming, typename Outgoing, typename InContainer, typename OutContainer, typename BufferT>
std::pair<StreamingIoPeer<Codec, Incoming, Outgoing, InContainer, OutContainer, BufferT>, Conveyor<HeapMessageRoot<Incoming, InContainer>>> newStreamingIoPeer(Own<AsyncIoStream> stream){
	auto caf = newConveyorAndFeeder<HeapMessageRoot<Incoming, InContainer>>();

	return {{std::move(caf.feeder), std::move(stream)}, std::move(caf.conveyor)};
}

}
