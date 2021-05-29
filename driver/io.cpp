#include "io.h"

namespace gin {
void ReadTaskAndStepHelper::readStep(StreamReader &reader) {
	if (read_ready) {
		read_ready->feed();
	}
	if (read_task.has_value()) {
		ReadIoTask &task = *read_task;

		ssize_t n = reader.readStream(task.buffer, task.max_length);

		if (n <= 0) {
			if (n == 0) {
				if (on_read_disconnect) {
					on_read_disconnect->feed();
				}
				return;
			}
			int error = errno;
			if (error == EAGAIN || error == EWOULDBLOCK) {
				return;
			} else {
				if (read_done) {
					read_done->fail(criticalError("Read failed"));
				}
				read_task = std::nullopt;
			}
		} else if (static_cast<size_t>(n) >= task.min_length &&
				   static_cast<size_t>(n) <= task.max_length) {
			if (read_done) {
				read_done->feed(static_cast<size_t>(n));
			}
			size_t max_len = task.max_length;
			read_task = std::nullopt;
		} else {
			task.buffer = reinterpret_cast<uint8_t *>(task.buffer) + n;
			task.min_length -= static_cast<size_t>(n);
			task.max_length -= static_cast<size_t>(n);
		}
	}
}

void WriteTaskAndStepHelper::writeStep(StreamWriter &writer) {
	if (write_ready) {
		write_ready->feed();
	}
	if (write_task.has_value()) {
		WriteIoTask &task = *write_task;

		ssize_t n = writer.writeStream(task.buffer, task.length);

		if (n < 0) {
			int error = errno;
			if (error == EAGAIN || error == EWOULDBLOCK) {
				return;
			} else {
				if (write_done) {
					write_done->fail(criticalError("Write failed"));
				}
				write_task = std::nullopt;
			}
		} else if (static_cast<size_t>(n) == task.length) {
			if (write_done) {
				write_done->feed(static_cast<size_t>(task.length));
			}
			write_task = std::nullopt;
		} else {
			task.buffer = reinterpret_cast<const uint8_t *>(task.buffer) +
						  static_cast<size_t>(n);
			task.length -= static_cast<size_t>(n);
		}
	}
}

} // namespace gin