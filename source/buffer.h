#pragma once

#include "error.h"

#include <array>
#include <cstdint>
#include <deque>
#include <list>
#include <string>
#include <vector>

namespace gin {
constexpr size_t RING_BUFFER_MAX_SIZE = 4096;
/*
 * Access class to reduce templated BufferSegments bloat
 */
class Buffer {
private:
	friend class RingBuffer;
	~Buffer() = default;

public:
	virtual size_t readPosition() const = 0;
	virtual size_t readCompositeLength() const = 0;
	virtual size_t readSegmentLength() const = 0;
	virtual void readAdvance(size_t bytes) = 0;

	virtual uint8_t &read(size_t i = 0) = 0;
	virtual const uint8_t &read(size_t i = 0) const = 0;

	virtual size_t writePosition() const = 0;
	virtual size_t writeCompositeLength() const = 0;
	virtual size_t writeSegmentLength() const = 0;
	virtual void writeAdvance(size_t bytes) = 0;

	virtual uint8_t &write(size_t i = 0) = 0;
	virtual const uint8_t &write(size_t i = 0) const = 0;

	virtual Error push(const uint8_t &value) = 0;
	virtual Error push(const uint8_t &buffer, size_t size) = 0;
	virtual Error pop(uint8_t &value) = 0;
	virtual Error pop(uint8_t &buffer, size_t size) = 0;

	virtual std::string toString() const = 0;
};
/*
 * Since we are only handing around data, a buffer pool would be quite nice
 */
/*
 * Buffers currently do not act as a ringbuffer even though that would be
 * possible since writing could be done up to the reading position and reading
 * can be done up to the current writing position Checks are in place to stop
 * that
 */
class RingBuffer final : public Buffer {
private:
	std::vector<uint8_t> buffer;
	size_t read_position;
	size_t write_position;
	bool write_reached_read = false;

public:
	RingBuffer();
	RingBuffer(size_t size);

	inline size_t size() const { return buffer.size(); }

	/*
	 * Shouldn't be used. Kinda unsafe if I shrink the buffer and don't adjust
	 * other values
	 */
	// inline void resize(size_t size) { buffer.resize(size); }

	inline uint8_t &operator[](size_t i) { return buffer[i]; }
	inline const uint8_t &operator[](size_t i) const { return buffer[i]; }

	size_t readPosition() const override;
	size_t readCompositeLength() const override;
	size_t readSegmentLength() const override;
	void readAdvance(size_t bytes) override;

	uint8_t &read(size_t i = 0) override;
	const uint8_t &read(size_t i = 0) const override;

	size_t writePosition() const override;
	size_t writeCompositeLength() const override;
	size_t writeSegmentLength() const override;
	void writeAdvance(size_t bytes) override;

	uint8_t &write(size_t i = 0) override;
	const uint8_t &write(size_t i = 0) const override;

	Error push(const uint8_t &value) override;
	Error push(const uint8_t &buffer, size_t size) override;
	Error pop(uint8_t &value) override;
	Error pop(uint8_t &buffer, size_t size) override;

	std::string toString() const override;
};

/*
 * One time buffer
 * Resettable by responsible instances
 */
class ArrayBuffer : public Buffer {
private:
	std::vector<uint8_t> buffer;

	size_t read_position;
	size_t write_position;

public:
	ArrayBuffer(size_t size);

	size_t readPosition() const override;
	size_t readCompositeLength() const override;
	size_t readSegmentLength() const override;
	void readAdvance(size_t bytes) override;

	uint8_t &read(size_t i = 0) override;
	const uint8_t &read(size_t i = 0) const override;

	size_t writePosition() const override;
	size_t writeCompositeLength() const override;
	size_t writeSegmentLength() const override;
	void writeAdvance(size_t bytes) override;

	uint8_t &write(size_t i = 0) override;
	const uint8_t &write(size_t i = 0) const override;

	Error push(const uint8_t &value) override;
	Error push(const uint8_t &buffer, size_t size) override;
	Error pop(uint8_t &value) override;
	Error pop(uint8_t &buffer, size_t size) override;
};

class ChainArrayBuffer : public Buffer {
private:
	std::deque<ArrayBuffer> buffer;

	size_t read_position;
	size_t write_position;

public:
	ChainArrayBuffer();

	size_t readPosition() const override;
	size_t readCompositeLength() const override;
	size_t readSegmentLength() const override;
	void readAdvance(size_t bytes) override;

	uint8_t &read(size_t i = 0) override;
	const uint8_t &read(size_t i = 0) const override;

	size_t writePosition() const override;
	size_t writeCompositeLength() const override;
	size_t writeSegmentLength() const override;
	void writeAdvance(size_t bytes) override;

	uint8_t &write(size_t i = 0) override;
	const uint8_t &write(size_t i = 0) const override;

	Error push(const uint8_t &value) override;
	Error push(const uint8_t &buffer, size_t size) override;
	Error pop(uint8_t &value) override;
	Error pop(uint8_t &buffer, size_t size) override;
};
} // namespace gin
