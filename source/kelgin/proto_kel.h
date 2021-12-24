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

	template <class Schema, class Container = MessageContainer<Schema>>
	Error encode(typename Message<Schema, Container>::Reader reader,
				 Buffer &buffer);

	template <class Schema, class Container = MessageContainer<Schema>>
	Error decode(typename Message<Schema, Container>::Builder builder,
				 Buffer &buffer, const Limits &limits = Limits{});
};

template <class T> struct ProtoKelEncodeImpl;

template <class T, size_t N, class Container>
struct ProtoKelEncodeImpl<Message<schema::Primitive<T, N>, Container>> {
	static Error
	encode(typename Message<schema::Primitive<T, N>, Container>::Reader data,
		   Buffer &buffer) {
		Error error = StreamValue<typename PrimitiveTypeHelper<
			schema::Primitive<T, N>>::Type>::encode(data.get(), buffer);
		return error;
	}

	static size_t
	size(typename Message<schema::Primitive<T, N>, Container>::Reader) {
		return StreamValue<typename PrimitiveTypeHelper<
			schema::Primitive<T, N>>::Type>::size();
	}
};

template <class Container>
struct ProtoKelEncodeImpl<Message<schema::String, Container>> {
	static Error
	encode(typename Message<schema::String, Container>::Reader data,
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

	static size_t
	size(typename Message<schema::String, Container>::Reader reader) {
		return sizeof(size_t) + reader.get().size();
	}
};

template <class... T, class Container>
struct ProtoKelEncodeImpl<Message<schema::Tuple<T...>, Container>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(T), Error>::type
	encodeMembers(typename Message<schema::Tuple<T...>, Container>::Reader,
				  Buffer &) {
		return noError();
	}

	template <size_t i = 0>
	static typename std::enable_if<(i < sizeof...(T)), Error>::type
	encodeMembers(typename Message<schema::Tuple<T...>, Container>::Reader data,
				  Buffer &buffer) {
		Error error =
			ProtoKelEncodeImpl<typename Container::template ElementType<i>>::
				encode(data.template get<i>(), buffer);
		if (error.failed()) {
			return error;
		}

		return encodeMembers<i + 1>(data, buffer);
	}

	static Error
	encode(typename Message<schema::Tuple<T...>, Container>::Reader data,
		   Buffer &buffer) {
		return encodeMembers<0>(data, buffer);
	}

	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(T), size_t>::type
	sizeMembers(typename Message<schema::Tuple<T...>, Container>::Reader) {
		return 0;
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(T), size_t>::type sizeMembers(
			typename Message<schema::Tuple<T...>, Container>::Reader reader) {
		return ProtoKelEncodeImpl<typename Container::template ElementType<i>>::
				   size(reader.template get<i>()) +
			   sizeMembers<i + 1>(reader);
	}

	static size_t
	size(typename Message<schema::Tuple<T...>, Container>::Reader reader) {
		return sizeMembers<0>(reader);
	}
};

template <typename... V, StringLiteral... K, class Container>
struct ProtoKelEncodeImpl<
	Message<schema::Struct<schema::NamedMember<V, K>...>, Container>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), Error>::type
	encodeMembers(typename Message<schema::Struct<schema::NamedMember<V, K>...>,
								   Container>::Reader,
				  Buffer &) {
		return noError();
	}
	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), Error>::type encodeMembers(
			typename Message<schema::Struct<schema::NamedMember<V, K>...>,
							 Container>::Reader data,
			Buffer &buffer) {

		Error error =
			ProtoKelEncodeImpl<typename Container::template ElementType<i>>::
				encode(data.template get<i>(), buffer);
		if (error.failed()) {
			return error;
		}
		return encodeMembers<i + 1>(data, buffer);
	}

	static Error
	encode(typename Message<schema::Struct<schema::NamedMember<V, K>...>,
							Container>::Reader data,
		   Buffer &buffer) {
		return encodeMembers<0>(data, buffer);
	}

	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), size_t>::type
	sizeMembers(typename Message<schema::Struct<schema::NamedMember<V, K>...>,
								 Container>::Reader) {
		return 0;
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), size_t>::type sizeMembers(
			typename Message<schema::Struct<schema::NamedMember<V, K>...>,
							 Container>::Reader reader) {
		return ProtoKelEncodeImpl<typename Container::template ElementType<i>>::
				   size(reader.template get<i>()) +
			   sizeMembers<i + 1>(reader);
	}

	static size_t
	size(typename Message<schema::Struct<schema::NamedMember<V, K>...>,
						  Container>::Reader reader) {
		return sizeMembers<0>(reader);
	}
};

template <typename... V, StringLiteral... K, class Container>
struct ProtoKelEncodeImpl<
	Message<schema::Union<schema::NamedMember<V, K>...>, Container>> {

	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), Error>::type
	encodeMembers(typename Message<schema::Union<schema::NamedMember<V, K>...>,
								   Container>::Reader,
				  Buffer &) {
		return noError();
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), Error>::type encodeMembers(
			typename Message<schema::Union<schema::NamedMember<V, K>...>,
							 Container>::Reader reader,
			Buffer &buffer) {
		if (reader.index() == i) {
			Error error = StreamValue<msg_union_id_t>::encode(i, buffer);
			if (error.failed()) {
				return error;
			}
			return ProtoKelEncodeImpl<typename Container::template ElementType<
				i>>::encode(reader.template get<i>(), buffer);
		}
		return encodeMembers<i + 1>(reader, buffer);
	}

	static Error
	encode(typename Message<schema::Union<schema::NamedMember<V, K>...>,
							Container>::Reader reader,
		   Buffer &buffer) {
		return encodeMembers<0>(reader, buffer);
	}

	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), size_t>::type
	sizeMembers(typename Message<schema::Union<schema::NamedMember<V, K>...>,
								 Container>::Reader) {
		return 0;
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), size_t>::type sizeMembers(
			typename Message<schema::Union<schema::NamedMember<V, K>...>,
							 Container>::Reader reader) {
		if (reader.index() == i) {
			return ProtoKelEncodeImpl<typename Container::template ElementType<
				i>>::size(reader.template get<i>());
		}
		return sizeMembers<i + 1>(reader);
	}

	/*
	 * Size of union id + member size
	 */
	static size_t
	size(typename Message<schema::Union<schema::NamedMember<V, K>...>,
						  Container>::Reader reader) {
		return sizeof(msg_union_id_t) + sizeMembers<0>(reader);
	}
};
/*
 * Decode Implementations
 */
template <typename T> struct ProtoKelDecodeImpl;

template <class T, size_t N, class Container>
struct ProtoKelDecodeImpl<Message<schema::Primitive<T, N>, Container>> {
	static Error
	decode(typename Message<schema::Primitive<T, N>, Container>::Builder data,
		   Buffer &buffer) {
		typename PrimitiveTypeHelper<schema::Primitive<T, N>>::Type val = 0;
		Error error = StreamValue<typename PrimitiveTypeHelper<
			schema::Primitive<T, N>>::Type>::decode(val, buffer);
		data.set(val);
		return error;
	}
};

template <class Container>
struct ProtoKelDecodeImpl<Message<schema::String, Container>> {
	static Error
	decode(typename Message<schema::String, Container>::Builder data,
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

template <class... T, class Container>
struct ProtoKelDecodeImpl<Message<schema::Tuple<T...>, Container>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(T), Error>::type
	decodeMembers(typename Message<schema::Tuple<T...>, Container>::Builder,
				  Buffer &) {
		return noError();
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(T), Error>::type decodeMembers(
			typename Message<schema::Tuple<T...>, Container>::Builder builder,
			Buffer &buffer) {

		Error error =
			ProtoKelDecodeImpl<typename Container::template ElementType<i>>::
				decode(builder.template init<i>(), buffer);
		if (error.failed()) {
			return error;
		}
		return decodeMembers<i + 1>(builder, buffer);
	}

	static Error
	decode(typename Message<schema::Tuple<T...>, Container>::Builder builder,
		   Buffer &buffer) {
		return decodeMembers<0>(builder, buffer);
	}
};

template <class... V, StringLiteral... K, class Container>
struct ProtoKelDecodeImpl<
	Message<schema::Struct<schema::NamedMember<V, K>...>, Container>> {

	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), Error>::type
	decodeMembers(typename Message<schema::Struct<schema::NamedMember<V, K>...>,
								   Container>::Builder,
				  Buffer &) {
		return noError();
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), Error>::type decodeMembers(
			typename Message<schema::Struct<schema::NamedMember<V, K>...>,
							 Container>::Builder builder,
			Buffer &buffer) {

		Error error =
			ProtoKelDecodeImpl<typename Container::template ElementType<i>>::
				decode(builder.template init<i>(), buffer);
		if (error.failed()) {
			return error;
		}
		return decodeMembers<i + 1>(builder, buffer);
	}

	static Error
	decode(typename Message<schema::Struct<schema::NamedMember<V, K>...>,
							Container>::Builder builder,
		   Buffer &buffer) {
		return decodeMembers<0>(builder, buffer);
	}
};

template <class... V, StringLiteral... K, class Container>
struct ProtoKelDecodeImpl<
	Message<schema::Union<schema::NamedMember<V, K>...>, Container>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), Error>::type
	decodeMembers(typename Message<schema::Union<schema::NamedMember<V, K>...>,
								   Container>::Builder,
				  Buffer &, msg_union_id_t) {
		return noError();
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), Error>::type decodeMembers(
			typename Message<schema::Union<schema::NamedMember<V, K>...>,
							 Container>::Builder builder,
			Buffer &buffer, msg_union_id_t id) {

		if (id == i) {
			Error error =
				ProtoKelDecodeImpl<typename Container::template ElementType<
					i>>::decode(builder.template init<i>(), buffer);
			if (error.failed()) {
				return error;
			}
		}
		return decodeMembers<i + 1>(builder, buffer, id);
	}

	static Error
	decode(typename Message<schema::Union<schema::NamedMember<V, K>...>,
							Container>::Builder builder,
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

template <class Schema, class Container>
Error ProtoKelCodec::encode(typename Message<Schema, Container>::Reader reader,
							Buffer &buffer) {
	BufferView view{buffer};

	msg_packet_length_t packet_length =
		ProtoKelEncodeImpl<Message<Schema, Container>>::size(reader);
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
		Error error = ProtoKelEncodeImpl<Message<Schema, Container>>::encode(
			reader, view);
		if (error.failed()) {
			return error;
		}
	}

	buffer.writeAdvance(view.writeOffset());
	return noError();
}

template <class Schema, class Container>
Error ProtoKelCodec::decode(
	typename Message<Schema, Container>::Builder builder, Buffer &buffer,
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
		Error error = ProtoKelDecodeImpl<Message<Schema, Container>>::decode(
			builder, view);
		if (error.failed()) {
			return error;
		}
	}
	{
		if (ProtoKelEncodeImpl<Message<Schema, Container>>::size(
				builder.asReader()) != packet_length) {
			return criticalError("Bad packet format");
		}
	}

	buffer.readAdvance(view.readOffset());
	return noError();
}

} // namespace gin
