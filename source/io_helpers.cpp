#include "io_helpers.h"

namespace gin {
void ReadTaskAndStepHelper::readStep(DataReader &reader) {
	if (read_ready) {
		read_ready->feed();
	}
	while (!read_tasks.empty()) {
		ReadIoTask &task = read_tasks.front();

		ssize_t n = reader.dataRead(task.buffer, task.max_length);

		if (n <= 0) {
			if (n == 0) {
				if (on_read_disconnect) {
					on_read_disconnect->feed();
				}
				break;
			}
			int error = errno;
			if (error == EAGAIN || error == EWOULDBLOCK) {
				break;
			} else {
				if (read_done) {
					read_done->fail(criticalError("Read failed"));
				}
				read_tasks.pop();
			}
		} else if (static_cast<size_t>(n) >= task.min_length &&
				   static_cast<size_t>(n) <= task.max_length) {
			if (read_done) {
				read_done->feed(static_cast<size_t>(n));
			}
			size_t max_len = task.max_length;
			read_tasks.pop();
		} else {
			task.buffer = reinterpret_cast<uint8_t *>(task.buffer) + n;
			task.min_length -= static_cast<size_t>(n);
			task.max_length -= static_cast<size_t>(n);
		}
	}
}

void WriteTaskAndStepHelper::writeStep(DataWriter &writer) {
	if (write_ready) {
		write_ready->feed();
	}
	while (!write_tasks.empty()) {
		WriteIoTask &task = write_tasks.front();

		ssize_t n = writer.dataWrite(task.buffer, task.length);

		if (n < 0) {
			int error = errno;
			if (error == EAGAIN || error == EWOULDBLOCK) {
				break;
			} else {
				if (write_done) {
					write_done->fail(criticalError("Write failed"));
				}
				write_tasks.pop();
			}
		} else if (static_cast<size_t>(n) == task.length) {
			if (write_done) {
				write_done->feed(static_cast<size_t>(task.length));
			}
			write_tasks.pop();
		} else {
			task.buffer = reinterpret_cast<const uint8_t *>(task.buffer) +
						  static_cast<size_t>(n);
			task.length -= static_cast<size_t>(n);
		}
	}
}

} // namespace gin