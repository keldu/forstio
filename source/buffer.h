#pragma once

#include <cstdint>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "error.h"

using std::size_t;

namespace gin {
/*
 * How do I store my data?
 * A list of std::arrays?
 */
class Buffer {
public:
	virtual ~Buffer() = default;

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

	virtual void pop() = 0;
	virtual bool push(uint8_t data) = 0;
	virtual size_t push(uint8_t *buffer, size_t size) = 0;

	virtual uint8_t &operator[](size_t i) = 0;
	virtual const uint8_t &operator[](size_t i) const = 0;

	virtual size_t size() const = 0;

	Error add(std::string_view rhs);

	std::string toString() const;
};

template <size_t S> class StaticArrayBuffer : public Buffer {
private:
	std::array<uint8_t, S> buffer_data;
};

class ArrayBuffer final : public Buffer {
private:
	std::vector<uint8_t> buffer_data;
	/*
	 * The index which describes the currently writable byte
	 */
	size_t write_index = 0;
	/*
	 * The index which describes the currently readable byte
	 */
	size_t read_index = 0;

public:
	ArrayBuffer();
	ArrayBuffer(size_t initial);

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

	void pop() override;
	bool push(uint8_t data) override;
	size_t push(uint8_t *buffer, size_t size) override;

	uint8_t &operator[](size_t i) override;
	const uint8_t &operator[](size_t i) const override;

	size_t size() const override;
};
} // namespace gin
