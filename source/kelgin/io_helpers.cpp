#include "io_helpers.h"

#include "io.h"

#include <cassert>

namespace gin {
void ReadTaskAndStepHelper::readStep(InputStream &reader) {
	while (read_task.has_value()) {
		ReadIoTask &task = *read_task;

		ErrorOr<size_t> n_err = reader.read(task.buffer, task.max_length);
		if (n_err.isError()) {
			const Error &error = n_err.error();
			if (error.isCritical()) {
				if (read_done) {
					read_done->fail(error.copyError());
				}
				read_task = std::nullopt;
			}

			break;
		} else if (n_err.isValue()) {
			size_t n = n_err.value();
			if (static_cast<size_t>(n) >= task.min_length &&
				static_cast<size_t>(n) <= task.max_length) {
				if (read_done) {
					read_done->feed(n + task.already_read);
				}
				read_task = std::nullopt;
			} else {
				task.buffer = static_cast<uint8_t *>(task.buffer) + n;
				task.min_length -= static_cast<size_t>(n);
				task.max_length -= static_cast<size_t>(n);
				task.already_read += n;
			}

		} else {
			if (read_done) {
				read_done->fail(criticalError("Read failed"));
			}
			read_task = std::nullopt;
		}
	}
}

void WriteTaskAndStepHelper::writeStep(OutputStream &writer) {
	while (write_task.has_value()) {
		WriteIoTask &task = *write_task;

		ErrorOr<size_t> n_err = writer.write(task.buffer, task.length);

		if (n_err.isValue()) {
			size_t n = n_err.value();
			assert(n <= task.length);
			if (n == task.length) {
				if (write_done) {
					write_done->feed(n + task.already_written);
				}
				write_task = std::nullopt;
			} else {
				task.buffer = static_cast<const uint8_t *>(task.buffer) + n;
				task.length -= n;
				task.already_written += n;
			}
		} else if (n_err.isError()) {
			const Error &error = n_err.error();
			if (error.isCritical()) {
				if (write_done) {
					write_done->fail(error.copyError());
				}
				write_task = std::nullopt;
			}
			break;
		} else {
			if (write_done) {
				write_done->fail(criticalError("Write failed"));
			}
			write_task = std::nullopt;
		}
	}
}

} // namespace gin