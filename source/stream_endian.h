#pragma once

#include "buffer.h"
#include "error.h"

#include <cstdint>
#include <cstring>

namespace gin {
template <typename T, size_t size = sizeof(T)> class ShiftStreamValue;

template <typename T> class ShiftStreamValue<T, 1> {
public:
	inline static Error decode(T &val, Buffer &buffer) {
		uint8_t& raw = reinterpret_cast<uint8_t&>(val);
		return buffer.pop(raw, sizeof(T));
	}

	inline static Error encode(const T &val, Buffer &buffer) {
		uint8_t& raw = reinterpret_cast<uint8_t&>(val);
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
		
		uint16_t& raw = reinterpret_cast<uint16_t&>(val);
		uint8_t buf[sizeof(T)] = 0;

		Error error = buffer.pop(buf, sizeof(T));
		if(error.failed()){
			return error;
		}

		for (size_t i = 0; i < sizeof(T); ++i) {
			raw |= buf[i] << (i * 8);
		}

		return noError();
	}

	inline static Error encode(const T &val, Buffer &buffer) {
		if (buffer.writeCompositeLength() < sizeof(T)) {
			return recoverableError("Buffer too small");
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
			raw |= buffer.read(i) << (i * 8);
		}
		memcpy(&val, &raw, sizeof(T));
		buffer.readAdvance(sizeof(T));

		return noError();
	}

	inline static Error encode(const T &val, Buffer &buffer) {
		if (buffer.writeCompositeLength() < sizeof(T)) {
			return recoverableError("Buffer too small");
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
			raw |= buffer.read(i) << (i * 8);
		}
		memcpy(&val, &raw, sizeof(T));
		buffer.readAdvance(sizeof(T));

		return noError();
	}

	inline static Error encode(const T &val, Buffer &buffer) {
		if (buffer.writeCompositeLength() < sizeof(T)) {
			return recoverableError("Buffer too small");
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

} // namespace gin