#pragma once

#include <kelgin/async.h>
#include <kelgin/common.h>

#include <cstdint>
#include <queue>

namespace gin {
/*
 * Helper classes for the specific driver implementations
 */

/*
 * Since I don't want to repeat these implementations for tls on unix systems
 * and gnutls doesn't let me write or read into buffers I have to have this kind
 * of strange abstraction. This may also be reusable for windows/macOS though.
 */

class DataReader {
public:
	virtual ~DataReader() = default;

	virtual ssize_t dataRead(void *buffer, size_t length) = 0;
};

class ReadTaskAndStepHelper {
public:
	struct ReadIoTask {
		void *buffer;
		size_t min_length;
		size_t max_length;
	};
	std::queue<ReadIoTask> read_tasks;
	Own<ConveyorFeeder<size_t>> read_done = nullptr;
	Own<ConveyorFeeder<void>> read_ready = nullptr;

	Own<ConveyorFeeder<void>> on_read_disconnect = nullptr;

public:
	void readStep(DataReader &reader);
};

class DataWriter {
public:
	virtual ~DataWriter() = default;

	virtual ssize_t dataWrite(const void *buffer, size_t length) = 0;
};

class WriteTaskAndStepHelper {
public:
	struct WriteIoTask {
		const void *buffer;
		size_t length;
	};
	std::queue<WriteIoTask> write_tasks;
	Own<ConveyorFeeder<size_t>> write_done = nullptr;
	Own<ConveyorFeeder<void>> write_ready = nullptr;

public:
	void writeStep(DataWriter &writer);
};

class DataReaderAndWriter : public DataReader, public DataWriter {
public:
	virtual ~DataReaderAndWriter() = default;
};
} // namespace gin