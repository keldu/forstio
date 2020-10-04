#pragma once

#include <kelgin/buffer.h>
#include <kelgin/error.h>

#include <cstdint>

namespace gin {
template <typename T, size_t size = sizeof(T)> class ShiftStreamValue;

template <typename T> class ShiftStreamValue<T, 1> {
public:
	inline static Error decode(T &val, Buffer &buffer) {
		if (buffer.readCompositeLength() < sizeof(T)) {
			return recoverableError("Buffer too small");
		}
		uint8_t raw = buffer.read();
		memcpy(&val, &raw, sizeof(T));
		buffer.readAdvance(sizeof(T));

		return noError();
	}

	inline static Error encode(T &val, Buffer &buffer) {
		if (buffer.writeCompositeLength() < sizeof(T)) {
			return recoverableError("Buffer too small");
		}
		uint8_t raw;
		memcpy(&raw, &val, sizeof(T));

		buffer.write() = raw;

		buffer.writeAdvance(sizeof(T));

		return noError();
	}
};

template <typename T> class ShiftStreamValue<T, 2> {
public:
	inline static Error decode(T &val, Buffer &buffer) {
		if (buffer.readCompositeLength() < sizeof(T)) {
			return recoverableError("Buffer too small");
		}

		uint16_t raw = 0;

		for (size_t i = 0; i < sizeof(T); ++i) {
			raw |= buffer.read(i) << (i * 8);
		}
		memcpy(&val, &raw, sizeof(T));
		buffer.readAdvance(sizeof(T));

		return noError();
	}

	inline static Error encode(T &val, Buffer &buffer) {
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

	inline static Error encode(T &val, Buffer &buffer) {
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

	inline static Error encode(T &val, Buffer &buffer) {
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
};

template <typename T> using StreamValue = ShiftStreamValue<T>;

} // namespace gin