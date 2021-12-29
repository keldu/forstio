#pragma once

#include "error.h"

#include <array>
#include <cstdint>
#include <deque>
#include <list>
#include <string>
#include <vector>

namespace saw {
/*
 * Access class to reduce templated BufferSegments bloat
 */
class Buffer {
protected:
	~Buffer() = default;

public:
	virtual size_t readPosition() const = 0;
	virtual size_t readCompositeLength() const = 0;
	virtual size_t readSegmentLength(size_t offset = 0) const = 0;
	virtual void readAdvance(size_t bytes) = 0;

	virtual uint8_t &read(size_t i = 0) = 0;
	virtual const uint8_t &read(size_t i = 0) const = 0;

	virtual size_t writePosition() const = 0;
	virtual size_t writeCompositeLength() const = 0;
	virtual size_t writeSegmentLength(size_t offset = 0) const = 0;
	virtual void writeAdvance(size_t bytes) = 0;

	virtual uint8_t &write(size_t i = 0) = 0;
	virtual const uint8_t &write(size_t i = 0) const = 0;

	/*
	 * Sometime buffers need to grow with a little more control
	 * than with push and pop for more efficient calls.
	 * There is nothing you can do if read hasn't been filled, but at
	 * least write can be increased if it is demanded.
	 */
	virtual Error writeRequireLength(size_t bytes) = 0;

	Error push(const uint8_t &value);
	Error push(const uint8_t &buffer, size_t size);
	Error pop(uint8_t &value);
	Error pop(uint8_t &buffer, size_t size);

	std::string toString() const;
	std::string toHex() const;
};

/*
 *	A viewer class for buffers.
 *	Working on the reference buffer invalidates the buffer view
 */
class BufferView : public Buffer {
private:
	Buffer &buffer;
	size_t read_offset;
	size_t write_offset;

public:
	BufferView(Buffer &);

	size_t readPosition() const override;
	size_t readCompositeLength() const override;
	size_t readSegmentLength(size_t offset = 0) const override;
	void readAdvance(size_t bytes) override;

	uint8_t &read(size_t i = 0) override;
	const uint8_t &read(size_t i = 0) const override;

	size_t writePosition() const override;
	size_t writeCompositeLength() const override;
	size_t writeSegmentLength(size_t offset = 0) const override;
	void writeAdvance(size_t bytes) override;

	uint8_t &write(size_t i = 0) override;
	const uint8_t &write(size_t i = 0) const override;

	Error writeRequireLength(size_t bytes) override;

	size_t readOffset() const;
	size_t writeOffset() const;
};

/*
 * Buffer size meant for default allocation size of the ringbuffer since
 * this class currently doesn't support proper resizing
 */
constexpr size_t RING_BUFFER_MAX_SIZE = 4096;
/*
 * Buffer wrapping around if read caught up
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

	inline uint8_t &operator[](size_t i) { return buffer[i]; }
	inline const uint8_t &operator[](size_t i) const { return buffer[i]; }

	size_t readPosition() const override;
	size_t readCompositeLength() const override;
	size_t readSegmentLength(size_t offset = 0) const override;
	void readAdvance(size_t bytes) override;

	uint8_t &read(size_t i = 0) override;
	const uint8_t &read(size_t i = 0) const override;

	size_t writePosition() const override;
	size_t writeCompositeLength() const override;
	size_t writeSegmentLength(size_t offset = 0) const override;
	void writeAdvance(size_t bytes) override;

	uint8_t &write(size_t i = 0) override;
	const uint8_t &write(size_t i = 0) const override;

	Error writeRequireLength(size_t bytes) override;
};

/*
 * One time buffer
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
	size_t readSegmentLength(size_t offset = 0) const override;
	void readAdvance(size_t bytes) override;

	uint8_t &read(size_t i = 0) override;
	const uint8_t &read(size_t i = 0) const override;

	size_t writePosition() const override;
	size_t writeCompositeLength() const override;
	size_t writeSegmentLength(size_t offset = 0) const override;
	void writeAdvance(size_t bytes) override;

	uint8_t &write(size_t i = 0) override;
	const uint8_t &write(size_t i = 0) const override;

	Error writeRequireLength(size_t bytes) override;
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
	size_t readSegmentLength(size_t offset = 0) const override;
	void readAdvance(size_t bytes) override;

	uint8_t &read(size_t i = 0) override;
	const uint8_t &read(size_t i = 0) const override;

	size_t writePosition() const override;
	size_t writeCompositeLength() const override;
	size_t writeSegmentLength(size_t offset = 0) const override;
	void writeAdvance(size_t bytes) override;

	uint8_t &write(size_t i = 0) override;
	const uint8_t &write(size_t i = 0) const override;

	Error writeRequireLength(size_t bytes) override;
};
} // namespace saw
