#pragma once

#include "async.h"
#include "common.h"

#include <cstdint>
#include <optional>

namespace saw {
/*
 * Helper classes for the specific driver implementations
 */

/*
 * Since I don't want to repeat these implementations for tls on unix systems
 * and gnutls doesn't let me write or read into buffers I have to have this kind
 * of strange abstraction. This may also be reusable for windows/macOS though.
 */
class InputStream;

class ReadTaskAndStepHelper {
public:
	struct ReadIoTask {
		void *buffer;
		size_t min_length;
		size_t max_length;
		size_t already_read = 0;
	};
	std::optional<ReadIoTask> read_task;
	Own<ConveyorFeeder<size_t>> read_done = nullptr;

	Own<ConveyorFeeder<void>> on_read_disconnect = nullptr;

public:
	void readStep(InputStream &reader);
};

class OutputStream;

class WriteTaskAndStepHelper {
public:
	struct WriteIoTask {
		const void *buffer;
		size_t length;
		size_t already_written = 0;
	};
	std::optional<WriteIoTask> write_task;
	Own<ConveyorFeeder<size_t>> write_done = nullptr;

public:
	void writeStep(OutputStream &writer);
};
} // namespace saw
