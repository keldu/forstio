#include "buffer.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace gin {
Error Buffer::push(const uint8_t &value) {
	size_t write_remain = writeCompositeLength();
	if (write_remain > 0) {
		write() = value;
		writeAdvance(1);
	} else {
		return recoverableError("Buffer too small");
	}
	return noError();
}

Error Buffer::push(const uint8_t &buffer, size_t size) {
	Error error = writeRequireLength(size);
	if (error.failed()) {
		return error;
	}
	const uint8_t *buffer_ptr = &buffer;
	while (size > 0) {
		size_t segment = std::min(writeSegmentLength(), size);
		memcpy(&write(), buffer_ptr, segment);
		writeAdvance(segment);
		size -= segment;
		buffer_ptr += segment;
	}
	return noError();
}

Error Buffer::pop(uint8_t &value) {
	if (readCompositeLength() > 0) {
		value = read();
		readAdvance(1);
	} else {
		return recoverableError("Buffer too small");
	}
	return noError();
}

Error Buffer::pop(uint8_t &buffer, size_t size) {
	if (readCompositeLength() >= size) {
		uint8_t *buffer_ptr = &buffer;
		while (size > 0) {
			size_t segment = std::min(readSegmentLength(), size);
			memcpy(buffer_ptr, &read(), segment);
			readAdvance(segment);
			size -= segment;
			buffer_ptr += segment;
		}
	} else {
		return recoverableError("Buffer too small");
	}
	return noError();
}

std::string Buffer::toString() const {
	std::ostringstream oss;
	for (size_t i = 0; i < readCompositeLength(); ++i) {
		oss << read(i);
	}
	return oss.str();
}

std::string Buffer::toHex() const {
	std::ostringstream oss;
	oss << std::hex << std::setfill('0');
	for (size_t i = 0; i < readCompositeLength(); ++i) {
		oss << std::setw(2) << (uint16_t)read(i);
		if ((i + 1) < readCompositeLength()) {
			oss << ((i % 4 == 3) ? '\n' : ' ');
		}
	}
	return oss.str();
}

RingBuffer::RingBuffer() : read_position{0}, write_position{0} {
	buffer.resize(RING_BUFFER_MAX_SIZE);
}

RingBuffer::RingBuffer(size_t size) : read_position{0}, write_position{0} {
	buffer.resize(size);
}

size_t RingBuffer::readPosition() const { return read_position; }

/*
 * If write is ahead of read it is a simple distance, but if read ist ahead of
 * write then there are two segments
 *
 */
size_t RingBuffer::readCompositeLength() const {
	return writePosition() < readPosition()
			   ? buffer.size() - (readPosition() - writePosition())
			   : (write_reached_read ? buffer.size()
									 : writePosition() - readPosition());
}

/*
 * If write is ahead then it's the simple distance again. If read is ahead it's
 * until the end of the buffer/segment
 */
size_t RingBuffer::readSegmentLength(size_t offset) const {
	size_t read_composite = readCompositeLength();
	assert(offset <= read_composite);
	offset = std::min(offset, read_composite);
	size_t remaining = read_composite - offset;

	size_t read_offset = readPosition() + offset;
	read_offset = read_offset >= buffer.size() ? read_offset - buffer.size()
											   : read_offset;

	// case 1 write is located before read and reached read
	// then offset can be used normally
	// case 2 write is located at read, but read reached write
	// then it is set to zero by readCompositeLength()
	// case 3 write is located after read
	// since std::min you can use simple subtraction
	if (writePosition() < read_offset) {
		return buffer.size() - read_offset;
	}

	if (writePosition() == read_offset) {
		if (remaining > 0) {
			return buffer.size() - read_offset;
		} else {
			return 0;
		}
	}

	return writePosition() - read_offset;
}

void RingBuffer::readAdvance(size_t bytes) {
	size_t read_composite = readCompositeLength();
	assert(bytes <= read_composite);
	bytes = std::min(bytes, read_composite);
	size_t advanced = read_position + bytes;
	read_position = advanced >= buffer.size() ? advanced - buffer.size()
											  : advanced;
	write_reached_read = bytes > 0 ? false : write_reached_read;
}

uint8_t &RingBuffer::read(size_t i) {
	assert(i < readCompositeLength());
	size_t pos = read_position + i;
	pos = pos >= buffer.size() ? pos - buffer.size() : pos;
	return buffer[pos];
}

const uint8_t &RingBuffer::read(size_t i) const {
	assert(i < readCompositeLength());
	size_t pos = read_position + i;
	pos = pos >= buffer.size() ? pos - buffer.size() : pos;
	return buffer[pos];
}

size_t RingBuffer::writePosition() const { return write_position; }

size_t RingBuffer::writeCompositeLength() const {
	return readPosition() > writePosition()
			   ? (readPosition() - writePosition())
			   : (write_reached_read
					  ? 0
					  : buffer.size() - (writePosition() - readPosition()));
}

size_t RingBuffer::writeSegmentLength(size_t offset) const {
	size_t write_composite = writeCompositeLength();
	assert(offset <= write_composite);
	offset = std::min(offset, write_composite);

	size_t write_offset = writePosition() + offset;
	write_offset = write_offset >= buffer.size() ? write_offset - buffer.size()
												 : write_offset;

	if (read_position > write_offset) {
		return read_position - write_offset;
	}

	if (write_reached_read) {
		return 0;
	}

	return buffer.size() - write_offset;
}

void RingBuffer::writeAdvance(size_t bytes) {
	assert(bytes <= writeCompositeLength());
	size_t advanced = write_position + bytes;
	write_position = advanced >= buffer.size() ? advanced - buffer.size()
											   : advanced;

	write_reached_read =
		(write_position == read_position && bytes > 0 ? true : false);
}

uint8_t &RingBuffer::write(size_t i) {
	assert(i < writeCompositeLength());
	size_t pos = write_position + i;
	pos = pos >= buffer.size() ? pos - buffer.size() : pos;
	return buffer[pos];
}

const uint8_t &RingBuffer::write(size_t i) const {
	assert(i < writeCompositeLength());
	size_t pos = write_position + i;
	pos = pos >= buffer.size() ? pos - buffer.size() : pos;
	return buffer[pos];
}
/*
	Error RingBuffer::increaseSize(size_t size){
		size_t old_size = buffer.size();
		size_t new_size = old_size + size;
		buffer.resize(new_size);
		if(readPosition() > writePosition() || (readPosition() ==
   writePosition() && write_reached_read)){ size_t remaining = old_size -
   writePosition(); size_t real_remaining = 0; while(remaining > 0){ size_t
   segment = std::min(remaining, size); memcpy(&buffer[new_size-segment],
   &buffer[old_size-segment], segment); remaining -= segment; size -= segment;
				old_size -= segment;
				new_size -= segment;
			}
		}

		return noError();
	}
*/
Error RingBuffer::writeRequireLength(size_t bytes) {
	size_t write_remain = writeCompositeLength();
	if (bytes > write_remain) {
		return recoverableError("Buffer too small");
	}
	return noError();
}

ArrayBuffer::ArrayBuffer(size_t size) : read_position{0}, write_position{0} {
	buffer.resize(size);
}

size_t ArrayBuffer::readPosition() const { return read_position; }

size_t ArrayBuffer::readCompositeLength() const {
	return write_position - read_position;
}

size_t ArrayBuffer::readSegmentLength(size_t offset) const {
	size_t read_composite = readCompositeLength();
	assert(offset <= read_composite);

	offset = std::min(read_composite, offset);
	size_t read_offset = read_position + offset;

	return write_position - read_offset;
}

void ArrayBuffer::readAdvance(size_t bytes) {
	assert(bytes <= readCompositeLength());
	read_position += bytes;
}

uint8_t &ArrayBuffer::read(size_t i) {
	assert(i < readCompositeLength());

	return buffer[i + read_position];
}

const uint8_t &ArrayBuffer::read(size_t i) const {
	assert(i + read_position < buffer.size());

	return buffer[i + read_position];
}

size_t ArrayBuffer::writePosition() const { return write_position; }

size_t ArrayBuffer::writeCompositeLength() const {
	assert(write_position <= buffer.size());
	return buffer.size() - write_position;
}

size_t ArrayBuffer::writeSegmentLength(size_t offset) const {
	assert(write_position <= buffer.size());
	size_t write_composite = writeCompositeLength();

	assert(offset <= write_composite);
	offset = std::min(write_composite, offset);
	size_t write_offset = write_position + offset;

	return buffer.size() - write_offset;
}

void ArrayBuffer::writeAdvance(size_t bytes) {
	assert(bytes <= writeCompositeLength());
	write_position += bytes;
}

uint8_t &ArrayBuffer::write(size_t i) {
	assert(i < writeCompositeLength());
	return buffer[i + write_position];
}

const uint8_t &ArrayBuffer::write(size_t i) const {
	assert(i < writeCompositeLength());
	return buffer[i + write_position];
}
Error ArrayBuffer::writeRequireLength(size_t bytes) {
	size_t write_remain = writeCompositeLength();
	if (bytes > write_remain) {
		return recoverableError("Buffer too small");
	}
	return noError();
}

} // namespace gin
