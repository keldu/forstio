#pragma once

#include "buffer.h"
#include "error.h"

#include <cstdint>
#include <cstring>

#include <iostream>

namespace saw {
/**
 * Helper class to encode/decode any primtive type into/from litte endian.
 * The shift class does this by shifting bytes. This type of procedure is
 * platform independent. So it does not matter if the memory layout is
 * little endian or big endian
 */
template <typename T, size_t size = sizeof(T)> class ShiftStreamValue;

template <typename T> class ShiftStreamValue<T, 1> {
public:
	inline static Error decode(T &val, Buffer &buffer) {
		uint8_t &raw = reinterpret_cast<uint8_t &>(val);
		return buffer.pop(raw, sizeof(T));
	}

	inline static Error encode(const T &val, Buffer &buffer) {
		const uint8_t &raw = reinterpret_cast<const uint8_t &>(val);
		return buffer.push(raw, sizeof(T));
	}

	inline static size_t size() { return sizeof(T); }
};

template <typename T> class ShiftStreamValue<T, 2> {
public:
	inline static Error decode(T &val, Buffer &buffer) {
		if (buffer.readCompositeLength() < sizeof(T)) {
			return recoverableError("Buffer too small");
		}

		uint16_t raw = 0;

		for (size_t i = 0; i < sizeof(T); ++i) {
			raw |= (static_cast<uint16_t>(buffer.read(i)) << (i * 8));
		}
		memcpy(&val, &raw, sizeof(T));
		buffer.readAdvance(sizeof(T));

		return noError();
	}

	inline static Error encode(const T &val, Buffer &buffer) {
		Error error = buffer.writeRequireLength(sizeof(T));
		if (error.failed()) {
			return error;
		}

		uint16_t raw;
		memcpy(&raw, &val, sizeof(T));

		for (size_t i = 0; i < sizeof(T); ++i) {
			buffer.write(i) = raw >> (i * 8);
		}

		buffer.writeAdvance(sizeof(T));
		return noError();
	}

	inline static size_t size() { return sizeof(T); }
};

template <typename T> class ShiftStreamValue<T, 4> {
public:
	inline static Error decode(T &val, Buffer &buffer) {
		if (buffer.readCompositeLength() < sizeof(T)) {
			return recoverableError("Buffer too small");
		}

		uint32_t raw = 0;

		for (size_t i = 0; i < sizeof(T); ++i) {
			raw |= (static_cast<uint32_t>(buffer.read(i)) << (i * 8));
		}
		memcpy(&val, &raw, sizeof(T));
		buffer.readAdvance(sizeof(T));

		return noError();
	}

	inline static Error encode(const T &val, Buffer &buffer) {
		Error error = buffer.writeRequireLength(sizeof(T));
		if (error.failed()) {
			return error;
		}

		uint32_t raw;
		memcpy(&raw, &val, sizeof(T));

		for (size_t i = 0; i < sizeof(T); ++i) {
			buffer.write(i) = raw >> (i * 8);
		}

		buffer.writeAdvance(sizeof(T));
		return noError();
	}

	inline static size_t size() { return sizeof(T); }
};

template <typename T> class ShiftStreamValue<T, 8> {
public:
	inline static Error decode(T &val, Buffer &buffer) {
		if (buffer.readCompositeLength() < sizeof(T)) {
			return recoverableError("Buffer too small");
		}

		uint64_t raw = 0;

		for (size_t i = 0; i < sizeof(T); ++i) {
			raw |= (static_cast<uint64_t>(buffer.read(i)) << (i * 8));
		}

		memcpy(&val, &raw, sizeof(T));
		buffer.readAdvance(sizeof(T));

		return noError();
	}

	inline static Error encode(const T &val, Buffer &buffer) {
		Error error = buffer.writeRequireLength(sizeof(T));
		if (error.failed()) {
			return error;
		}

		uint64_t raw;
		memcpy(&raw, &val, sizeof(T));

		for (size_t i = 0; i < sizeof(T); ++i) {
			buffer.write(i) = raw >> (i * 8);
		}

		buffer.writeAdvance(sizeof(T));
		return noError();
	}

	inline static size_t size() { return sizeof(T); }
};

template <typename T> using StreamValue = ShiftStreamValue<T>;

} // namespace saw
