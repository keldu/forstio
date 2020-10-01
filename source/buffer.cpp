#include "buffer.h"

#include <cassert>

#include <sstream>

namespace gin {
Error Buffer::add(std::string_view rhs) {
	for (size_t i = 0; i < rhs.size(); ++i) {
		if (!push((static_cast<uint8_t>(rhs[i])))) {
			return recoverableError("Couldn't push to buffer");
		}
	}
	return noError();
}

std::string Buffer::toString() const {
	std::stringstream iss;
	for (size_t i = 0; i < size(); ++i) {
		iss << (*this)[i];
	}
	return iss.str();
}

ArrayBuffer::ArrayBuffer() {}

ArrayBuffer::ArrayBuffer(size_t size) { buffer_data.reserve(size); }

size_t ArrayBuffer::readPosition() const { return read_index; }

size_t ArrayBuffer::readCompositeLength() const {
	return write_index - read_index;
}

size_t ArrayBuffer::readSegmentLength() const {
	return write_index - read_index;
}

void ArrayBuffer::readAdvance(size_t bytes) {
	assert(bytes <= readCompositeLength());
	read_index += bytes;
}

uint8_t &ArrayBuffer::read(size_t i) { return buffer_data.at(read_index + i); }

const uint8_t &ArrayBuffer::read(size_t i) const {
	return buffer_data.at(read_index + i);
}

size_t ArrayBuffer::writePosition() const { return write_index; }

size_t ArrayBuffer::writeCompositeLength() const {
	return buffer_data.size() - write_index;
}

size_t ArrayBuffer::writeSegmentLength() const {
	return buffer_data.size() - write_index;
}

void ArrayBuffer::writeAdvance(size_t bytes) {
	assert(bytes <= writeCompositeLength());
	write_index += bytes;
}

uint8_t &ArrayBuffer::write(size_t i) {
	return buffer_data.at(write_index + i);
}

const uint8_t &ArrayBuffer::write(size_t i) const {
	return buffer_data.at(write_index + i);
}

void ArrayBuffer::pop() { readAdvance(readCompositeLength() > 0 ? 1 : 0); }

bool ArrayBuffer::push(uint8_t data) {
	if (writeCompositeLength() > 0) {
		write() = data;
		writeAdvance(1);
	} else {
		buffer_data.push_back(data);
		writeAdvance(1);
	}

	return true;
}

size_t ArrayBuffer::push(uint8_t *buffer, size_t size) {
	size_t remaining = size;
	if (writeCompositeLength() < size) {
		buffer_data.resize(buffer_data.size() + size);
	}

	for (size_t i = 0; i < size; ++i) {
		write(i) = buffer[i];
	}
	writeAdvance(size);

	return size;
}

uint8_t &ArrayBuffer::operator[](size_t i) { return buffer_data[i]; }

const uint8_t &ArrayBuffer::operator[](size_t i) const {
	return buffer_data[i];
}

size_t ArrayBuffer::size() const { return buffer_data.size(); }
} // namespace gin