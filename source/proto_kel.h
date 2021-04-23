#pragma once

#include "buffer.h"
#include "message.h"
#include "stream_endian.h"

#include <iostream>

namespace gin {
/// @todo replace types with these
/*
 * I'm not really sure if anyone will use a union which is
 * bigger than uint32_t max. At least I hope noone would do this
 */
using msg_union_id_t = uint32_t;
using msg_packet_length_t = uint64_t;

class ProtoKelCodec {
private:
	struct ReadContext {
		Buffer &buffer;
		size_t offset = 0;
	};

	template <typename T> friend struct ProtoKelEncodeImpl;

	template <typename T> friend struct ProtoKelDecodeImpl;

public:
	struct Limits {
		msg_packet_length_t packet_size;

		Limits() : packet_size{4096} {}
		Limits(msg_packet_length_t ps) : packet_size{ps} {}
	};

	struct Version {
		size_t major;
		size_t minor;
		size_t security;
	};

	const Version version() const { return Version{0, 0, 0}; }

	template <typename T>
	Error encode(typename T::Reader reader, Buffer &buffer);

	template <typename T>
	Error decode(typename T::Builder builder, Buffer &buffer,
				 const Limits &limits = Limits{});
};

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

		Error error = buffer.writeRequireLength(sizeof(size) + size);
		if (error.failed()) {
			return error;
		}

		error = StreamValue<size_t>::encode(size, buffer);
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
	sizeMembers(typename MessageList<T...>::Reader) {
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
			Error error = StreamValue<msg_union_id_t>::encode(i, buffer);
			if (error.failed()) {
				return error;
			}
			return ProtoKelEncodeImpl<typename ParameterPackType<
				i, V...>::type>::encode(reader.template get<i>(), buffer);
		}
		return encodeMembers<i + 1>(reader, buffer);
	}

	static Error
	encode(typename MessageUnion<MessageUnionMember<V, K>...>::Reader reader,
		   Buffer &buffer) {
		return encodeMembers<0>(reader, buffer);
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

	/*
	 * Size of union id + member size
	 */
	static size_t
	size(typename MessageUnion<MessageUnionMember<V, K>...>::Reader reader) {
		return sizeof(uint32_t) + sizeMembers<0>(reader);
	}
};
/*
 * Decode Implementations
 */
template <typename T> struct ProtoKelDecodeImpl;

template <typename T> struct ProtoKelDecodeImpl<MessagePrimitive<T>> {
	static Error decode(typename MessagePrimitive<T>::Builder data,
						Buffer &buffer) {
		T val = 0;
		Error error = StreamValue<T>::decode(val, buffer);
		data.set(val);
		return error;
	}
};

template <> struct ProtoKelDecodeImpl<MessagePrimitive<std::string>> {
	static Error decode(typename MessagePrimitive<std::string>::Builder data,
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
		data.set(std::move(value));
		return noError();
	}
};

template <typename... T> struct ProtoKelDecodeImpl<MessageList<T...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(T), Error>::type
	decodeMembers(typename MessageList<T...>::Builder, Buffer &) {
		return noError();
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(T), Error>::type
		decodeMembers(typename MessageList<T...>::Builder builder,
					  Buffer &buffer) {

		Error error =
			ProtoKelDecodeImpl<typename ParameterPackType<i, T...>::type>::
				decode(builder.template init<i>(), buffer);
		if (error.failed()) {
			return error;
		}
		return decodeMembers<i + 1>(builder, buffer);
	}

	static Error decode(typename MessageList<T...>::Builder builder,
						Buffer &buffer) {
		return decodeMembers<0>(builder, buffer);
	}
};

template <typename... V, typename... K>
struct ProtoKelDecodeImpl<MessageStruct<MessageStructMember<V, K>...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), Error>::type
	decodeMembers(typename MessageStruct<MessageStructMember<V, K>...>::Builder,
				  Buffer &) {
		return noError();
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), Error>::type decodeMembers(
			typename MessageStruct<MessageStructMember<V, K>...>::Builder
				builder,
			Buffer &buffer) {

		Error error =
			ProtoKelDecodeImpl<typename ParameterPackType<i, V...>::type>::
				decode(builder.template init<i>(), buffer);
		if (error.failed()) {
			return error;
		}
		return decodeMembers<i + 1>(builder, buffer);
	}

	static Error decode(
		typename MessageStruct<MessageStructMember<V, K>...>::Builder builder,
		Buffer &buffer) {
		return decodeMembers<0>(builder, buffer);
	}
};

template <typename... V, typename... K>
struct ProtoKelDecodeImpl<MessageUnion<MessageUnionMember<V, K>...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), Error>::type
	decodeMembers(typename MessageUnion<MessageUnionMember<V, K>...>::Builder,
				  Buffer &, msg_union_id_t) {
		return noError();
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), Error>::type decodeMembers(
			typename MessageUnion<MessageUnionMember<V, K>...>::Builder builder,
			Buffer &buffer, msg_union_id_t id) {

		if (id == i) {
			Error error =
				ProtoKelDecodeImpl<typename ParameterPackType<i, V...>::type>::
					decode(builder.template init<i>(), buffer);
			if (error.failed()) {
				return error;
			}
		}
		return decodeMembers<i + 1>(builder, buffer, id);
	}

	static Error
	decode(typename MessageUnion<MessageUnionMember<V, K>...>::Builder builder,
		   Buffer &buffer) {
		msg_union_id_t id = 0;
		Error error = StreamValue<msg_union_id_t>::decode(id, buffer);
		if (error.failed()) {
			return error;
		}
		if (id >= sizeof...(V)) {
			return criticalError("Union doesn't have this many id's");
		}

		return decodeMembers<0>(builder, buffer, id);
	}
};

template <typename T>
Error ProtoKelCodec::encode(typename T::Reader reader, Buffer &buffer) {
	BufferView view{buffer};

	msg_packet_length_t packet_length = ProtoKelEncodeImpl<T>::size(reader);
	// Check the size of the packet for the first
	// message length description

	Error error =
		view.writeRequireLength(packet_length + sizeof(msg_packet_length_t));
	if (error.failed()) {
		return error;
	}

	{
		Error error =
			StreamValue<msg_packet_length_t>::encode(packet_length, view);
		if (error.failed()) {
			return error;
		}
	}
	{
		Error error = ProtoKelEncodeImpl<T>::encode(reader, view);
		if (error.failed()) {
			return error;
		}
	}

	buffer.writeAdvance(view.writeOffset());
	return noError();
}

template <typename T>
Error ProtoKelCodec::decode(typename T::Builder builder, Buffer &buffer,
							const Limits &limits) {
	BufferView view{buffer};

	msg_packet_length_t packet_length = 0;
	{
		Error error =
			StreamValue<msg_packet_length_t>::decode(packet_length, view);
		if (error.failed()) {
			return error;
		}
	}

	if (packet_length > limits.packet_size) {
		return criticalError("Packet size too big");
	}

	{
		Error error = ProtoKelDecodeImpl<T>::decode(builder, view);
		if (error.failed()) {
			return error;
		}
	}
	{
		if (ProtoKelEncodeImpl<T>::size(builder.asReader()) != packet_length) {
			return criticalError("Bad packet format");
		}
	}

	buffer.readAdvance(view.readOffset());
	return noError();
}

} // namespace gin