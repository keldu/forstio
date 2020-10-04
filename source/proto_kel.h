#pragma once

#include "stream_endian.h"

namespace gin {
template <typename T> struct ProtoKelEncodeImpl;

template <typename T> struct ProtoKelEncodeImpl<MessagePrimitive<T>> {
	static Error encode(typename MessagePrimitive<T>::Reader data,
						Buffer &buffer) {

		Error error = StreamValue<T>::encode(data.get(), buffer);
		return error;
	}
};

template <> struct ProtoKelEncodeImpl<MessagePrimitive<std::string>> {
	static Error encode(typename MessagePrimitive<std::string>::Reader data,
						Buffer &buffer) {
		std::string_view view = data.get();
		size_t size = view.size();
		if ((sizeof(size) + size) > buffer.writeCompositeLength()) {
			return recoverableError("Buffer too small");
		}
		Error error = StreamValue<size_t>::encode(size, buffer);
		if (error.failed()) {
			return error;
		}
		for (size_t i = 0; i < view.size(); ++i) {
			buffer.write(i) = view[i];
		}
		buffer.writeAdvance(view.size());
		return noError();
	}
};

template <typename... T> struct ProtoKelEncodeImpl<MessageList<T...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(T), Error>::type
	encodeMembers(typename MessageList<T...>::Reader, Buffer &) {
		return Error{};
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(T), Error>::type
		encodeMembers(typename MessageList<T...>::Reader data, Buffer &buffer) {
		// Error error =
		return encodeMembers<i + 1>(data, buffer);
	}

	static Error encode(typename MessageList<T...>::Reader data,
						Buffer &buffer) {}
};

template <typename T> struct ProtoKelDecodeImpl;

template <typename T> struct ProtoKelDecodeImpl<MessagePrimitive<T>> {
	static Error encode(typename MessagePrimitive<T>::Builder data,
						Buffer &buffer) {

		Error error = StreamValue<T>::decode(data.get(), buffer);
		return error;
	}
};

template <> struct ProtoKelDecodeImpl<MessagePrimitive<std::string>> {
	static Error encode(typename MessagePrimitive<std::string>::Builder data,
						Buffer &buffer) {
		size_t size = 0;
		if (sizeof(size) > buffer.readCompositeLength()) {
			return recoverableError("Buffer too small");
		}

		Error error = StreamValue<size_t>::decode(size, buffer);
		if (error.failed()) {
			return error;
		}

		if (size > buffer.readCompositeLength()) {
			return recoverableError("Buffer too small");
		}

		std::string value;
		value.resize(size);

		if (size > buffer.readCompositeLength()) {
			return recoverableError("Buffer too small");
		}
		for (size_t i = 0; i < value.size(); ++i) {
			value[i] = buffer.read(i);
		}
		buffer.readAdvance(view.size());
		return noError();
	}
};

} // namespace gin