#pragma once

#include <kelgin/async.h>
#include <kelgin/io.h>
#include <kelgin/common.h>

#include <cstdint>
#include <optional>

namespace gin {
/*
 * Helper classes for the specific driver implementations
 */

/*
 * Since I don't want to repeat these implementations for tls on unix systems
 * and gnutls doesn't let me write or read into buffers I have to have this kind
 * of strange abstraction. This may also be reusable for windows/macOS though.
 */

class StreamReader {
protected:
	~StreamReader() = default;
public:
	virtual ssize_t readStream(void* buffer, size_t length) = 0;
};

class ReadTaskAndStepHelper {
public:
	struct ReadIoTask {
		void *buffer;
		size_t min_length;
		size_t max_length;
	};
	std::optional<ReadIoTask> read_task;
	Own<ConveyorFeeder<size_t>> read_done = nullptr;
	Own<ConveyorFeeder<void>> read_ready = nullptr;

	Own<ConveyorFeeder<void>> on_read_disconnect = nullptr;

public:
	void readStep(StreamReader &reader);
};

class StreamWriter {
protected:
	~StreamWriter() = default;
public:
	virtual ssize_t writeStream(const void* buffer, size_t length) = 0;
};

class WriteTaskAndStepHelper {
public:
	struct WriteIoTask {
		const void *buffer;
		size_t length;
	};
	std::optional<WriteIoTask> write_task;
	Own<ConveyorFeeder<size_t>> write_done = nullptr;
	Own<ConveyorFeeder<void>> write_ready = nullptr;

public:
	void writeStep(StreamWriter &writer);
};

class StreamReaderAndWriter : public StreamReader, public StreamWriter {
protected:
	~StreamReaderAndWriter() = default;
};
} // namespace gin