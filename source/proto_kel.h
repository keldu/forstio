#pragma once

#include "buffer.h"
#include "message.h"
#include "stream_endian.h"

#include <iostream>

namespace gin {
template <typename T> struct ProtoKelEncodeImpl;

template <typename T> struct ProtoKelEncodeImpl<MessagePrimitive<T>> {
	static Error encode(typename MessagePrimitive<T>::Reader data,
						Buffer &buffer) {
		Error error = StreamValue<T>::encode(data.get(), buffer);
		return error;
	}

	static size_t size(typename MessagePrimitive<T>::Reader) {
		return StreamValue<T>::size();
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

	static size_t size(typename MessagePrimitive<std::string>::Reader reader) {
		return sizeof(size_t) + reader.get().size();
	}
};

template <typename... T> struct ProtoKelEncodeImpl<MessageList<T...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(T), Error>::type
	encodeMembers(typename MessageList<T...>::Reader, Buffer &) {
		return noError();
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(T), Error>::type
		encodeMembers(typename MessageList<T...>::Reader data, Buffer &buffer) {
		Error error =
			ProtoKelEncodeImpl<typename ParameterPackType<i, T...>::type>::
				encode(data.template get<i>(), buffer);
		if (error.failed()) {
			return error;
		}

		return encodeMembers<i + 1>(data, buffer);
	}

	static Error encode(typename MessageList<T...>::Reader data,
						Buffer &buffer) {
		return encodeMembers<0>(data, buffer);
	}

	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(T), size_t>::type
	sizeMembers(typename MessageList<T...>::Reader data) {
		return 0;
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(T), size_t>::type
		sizeMembers(typename MessageList<T...>::Reader reader) {
		return ProtoKelEncodeImpl<typename ParameterPackType<i, T...>::type>::
				   size(reader.template get<i>()) +
			   sizeMembers<i + 1>(reader);
	}

	static size_t size(typename MessageList<T...>::Reader reader) {
		return sizeMembers<0>(reader);
	}
};

template <typename... V, typename... K>
struct ProtoKelEncodeImpl<MessageStruct<MessageStructMember<V, K>...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), Error>::type
	encodeMembers(typename MessageStruct<MessageStructMember<V, K>...>::Reader,
				  Buffer &) {
		return noError();
	}
	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), Error>::type encodeMembers(
			typename MessageStruct<MessageStructMember<V, K>...>::Reader data,
			Buffer &buffer) {

		Error error =
			ProtoKelEncodeImpl<typename ParameterPackType<i, V...>::type>::
				encode(data.template get<i>(), buffer);
		if (error.failed()) {
			return error;
		}
		return encodeMembers<i + 1>(data, buffer);
	}

	static Error
	encode(typename MessageStruct<MessageStructMember<V, K>...>::Reader data,
		   Buffer &buffer) {
		return encodeMembers<0>(data, buffer);
	}

	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), size_t>::type
	sizeMembers(typename MessageStruct<MessageStructMember<V, K>...>::Reader) {
		return 0;
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), size_t>::type
		sizeMembers(typename MessageStruct<MessageStructMember<V, K>...>::Reader
						reader) {
		return ProtoKelEncodeImpl<typename ParameterPackType<i, V...>::type>::
				   size(reader.template get<i>()) +
			   sizeMembers<i + 1>(reader);
	}

	static size_t
	size(typename MessageStruct<MessageStructMember<V, K>...>::Reader reader) {
		return sizeMembers<0>(reader);
	}
};

template <typename... V, typename... K>
struct ProtoKelEncodeImpl<MessageUnion<MessageUnionMember<V, K>...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), Error>::type
	encodeMembers(typename MessageUnion<MessageUnionMember<V, K>...>::Reader,
				  Buffer &) {
		return noError();
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), Error>::type encodeMembers(
			typename MessageUnion<MessageUnionMember<V, K>...>::Reader reader,
			Buffer &buffer) {
		if (reader.template holdsAlternative<
				typename ParameterPackType<i, K...>::type>()) {
			return ProtoKelEncodeImpl<typename ParameterPackType<
				i, V...>::type>::encode(reader.template get<i>(), buffer);
		}
		return encodeMembers<i + 1>(reader, buffer);
	}

	static Error
	encode(typename MessageUnion<MessageUnionMember<V, K>...>::Reader data,
		   Buffer &buffer) {
		return encodeMembers<0>(data, buffer);
	}

	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), size_t>::type
	sizeMembers(typename MessageUnion<MessageUnionMember<V, K>...>::Reader) {
		return 0;
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), size_t>::type sizeMembers(
			typename MessageUnion<MessageUnionMember<V, K>...>::Reader reader) {
		if (reader.template holdsAlternative<
				typename ParameterPackType<i, K...>::type>()) {
			return ProtoKelEncodeImpl<typename ParameterPackType<
				i, V...>::type>::size(reader.template get<i>());
		}
		return sizeMembers<i + 1>(reader);
	}

	static size_t
	size(typename MessageUnion<MessageUnionMember<V, K>...>::Reader reader) {
		return sizeMembers<0>(reader);
	}
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
		buffer.readAdvance(value.size());
		return noError();
	}
};

class ProtoKelCodec {
public:
	template <typename T>
	Error encode(typename T::Reader reader, Buffer &buffer) {
		uint64_t packet_length = ProtoKelEncodeImpl<T>::size(reader);
		// Check the size of the packet for the first
		// message length description

		if (buffer.writeCompositeLength() <
			(packet_length + sizeof(uint64_t))) {
			return recoverableError("Buffer too small");
		}
		{
			Error error = StreamValue<uint64_t>::encode(packet_length, buffer);
			if (error.failed()) {
				return error;
			}
		}
		{
			Error error = ProtoKelEncodeImpl<T>::encode(reader, buffer);
			if (error.failed()) {
				return error;
			}
		}

		return noError();
	};

	template <typename T>
	Error decode(typename T::Builder builder, Buffer &buffer) {
		return noError();
	}
};

} // namespace gin