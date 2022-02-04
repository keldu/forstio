#include "io.h"

#include <cassert>

namespace saw {

AsyncIoStream::AsyncIoStream(Own<IoStream> str)
	: stream{std::move(str)}, read_ready{stream->readReady()
											 .then([this]() {
												 read_stepper.readStep(*stream);
											 })
											 .sink()},
	  write_ready{stream->writeReady()
					  .then([this]() { write_stepper.writeStep(*stream); })
					  .sink()},
	  read_disconnected{stream->onReadDisconnected()
							.then([this]() {
								if (read_stepper.on_read_disconnect) {
									read_stepper.on_read_disconnect->feed();
								}
							})
							.sink()} {}

void AsyncIoStream::read(void *buffer, size_t min_length, size_t max_length) {
	SAW_ASSERT(buffer && max_length >= min_length && min_length > 0) { return; }

	SAW_ASSERT(!read_stepper.read_task.has_value()) { return; }

	read_stepper.read_task =
		ReadTaskAndStepHelper::ReadIoTask{buffer, min_length, max_length, 0};
	read_stepper.readStep(*stream);
}

Conveyor<size_t> AsyncIoStream::readDone() {
	auto caf = newConveyorAndFeeder<size_t>();
	read_stepper.read_done = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

Conveyor<void> AsyncIoStream::onReadDisconnected() {
	auto caf = newConveyorAndFeeder<void>();
	read_stepper.on_read_disconnect = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

void AsyncIoStream::write(const void *buffer, size_t length) {
	SAW_ASSERT(buffer && length > 0) { return; }

	SAW_ASSERT(!write_stepper.write_task.has_value()) { return; }

	write_stepper.write_task =
		WriteTaskAndStepHelper::WriteIoTask{buffer, length, 0};
	write_stepper.writeStep(*stream);
}

Conveyor<size_t> AsyncIoStream::writeDone() {
	auto caf = newConveyorAndFeeder<size_t>();
	write_stepper.write_done = std::move(caf.feeder);
	return std::move(caf.conveyor);
}

StringNetworkAddress::StringNetworkAddress(const std::string &address,
										   uint16_t port)
	: address_value{address}, port_value{port} {}

const std::string &StringNetworkAddress::address() const {
	return address_value;
}

uint16_t StringNetworkAddress::port() const { return port_value; }
} // namespace saw
