#include "io.h"

namespace gin {

AsyncIoStream::AsyncIoStream(Own<IoStream> str)
	: stream{std::move(str)},
	  read_ready{stream->readReady().then([this]() {}).sink()},
	  write_ready{stream->writeReady()
					  .then([this]() {

					  })
					  .sink()},
	  read_disconnected{stream->onReadDisconnected()
							.then([this]() {

							})
							.sink()} {}

void AsyncIoStream::read(void *buffer, size_t min_length, size_t max_length) {}

Conveyor<size_t> AsyncIoStream::readDone() {
	auto caf = newConveyorAndFeeder<size_t>();
	read_stepper.read_done = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

void AsyncIoStream::write(const void *buffer, size_t length) {}

Conveyor<size_t> AsyncIoStream::writeDone() {
	auto caf = newConveyorAndFeeder<size_t>();
	write_stepper.write_done = std::move(caf.feeder);
	return std::move(caf.conveyor);
}
} // namespace gin