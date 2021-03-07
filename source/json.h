#pragma once

#include "buffer.h"
#include "message.h"
#include "message_dynamic.h"

#include "error.h"

#include <cassert>
#include <charconv>
#include <cstring>
#include <sstream>
#include <string_view>
#include <tuple>

#include <iostream>

namespace gin {
class JsonCodec {
public:
	struct Limits {
		size_t depth = 16;
		size_t elements = 1024;
	};

private:
	struct WriteContext {
	public:
		Buffer &buffer;

		size_t offset = 0;
		size_t depth = 0;
		size_t elements = 0;

		Error push(const uint8_t &buffer_ref, size_t size) {
			Error error = buffer.writeRequireLength(offset + size);
			if (error.failed()) {
				return error;
			}

			const uint8_t *buffer_ptr = &buffer_ref;
			size_t write_left = size;
			while (write_left > 0) {
				size_t segment =
					std::min(buffer.writeSegmentLength(offset), write_left);
				if (segment == 0) {
					return recoverableError("Buffer segment size 0");
				}
				memcpy(&buffer.write(offset), buffer_ptr, segment);

				offset += segment;
				write_left -= segment;
				buffer_ptr += segment;
				/// @todo i need to know the offset write segment length before
				/// write has been advanced :/
				/// @todo change the buffer interface to use an offset with
				/// default size_t offset = 0
			}

			return noError();
		}

		Error push(const uint8_t &value) { return push(value, 1); }
	};
	template <typename T> friend class JsonEncodeImpl;

	struct ReadContext {
	public:
		Buffer &buffer;
		const Limits &limits;
		size_t offset = 0;
		size_t depth = 0;
		size_t elements = 0;

		size_t readLength() const {
			size_t remaining = buffer.readCompositeLength();
			assert(offset <= remaining);
			return offset > remaining ? 0 : remaining - offset;
		}
		uint8_t &read(size_t i = 0) {
			assert(readLength() > i);

			return buffer.read(offset + i);
		}
		void readAdvance(size_t bytes) {
			size_t remaining = readLength();
			assert(remaining >= bytes);

			bytes = std::min(bytes, remaining);
			offset += bytes;
		}
	};

	template <typename T> friend class JsonDecodeImpl;

	bool isWhitespace(int8_t letter);

	void skipWhitespace(ReadContext &);

	Error decodeBool(DynamicMessageBool::Builder message, ReadContext &);

	// Not yet clear if this number is a double or an integer
	Error decodeNumber(Own<DynamicMessage> &message, ReadContext &);

	Error decodeNull(ReadContext &);

	Error decodeValue(Own<DynamicMessage> &message, ReadContext &);

	Error decodeRawString(std::string &raw, ReadContext &);

	Error decodeList(DynamicMessageList::Builder builder, ReadContext &);

	Error decodeStruct(DynamicMessageStruct::Builder message, ReadContext &);

	ErrorOr<Own<DynamicMessage>> decodeDynamic(ReadContext &);

public:
	template <typename T>
	Error encode(typename T::Reader reader, Buffer &buffer);

	template <typename T>
	Error decode(typename T::Builder builder, Buffer &buffer,
				 const Limits &limits = Limits{16, 1024});
};

template <typename T> struct JsonEncodeImpl;

template <typename T> struct JsonEncodeImpl<MessagePrimitive<T>> {
	static Error encode(typename MessagePrimitive<T>::Reader data,
						JsonCodec::WriteContext &buffer) {
		std::string stringified = std::to_string(data.get());
		Error error =
			buffer.push(*reinterpret_cast<const uint8_t *>(stringified.data()),
						stringified.size());
		if (error.failed()) {
			return error;
		}
		return noError();
	}
};

template <> struct JsonEncodeImpl<MessagePrimitive<std::string>> {
	static Error encode(typename MessagePrimitive<std::string>::Reader data,
						JsonCodec::WriteContext &buffer) {
		std::string str =
			std::string{"\""} + std::string{data.get()} + std::string{"\""};
		Error error = buffer.push(
			*reinterpret_cast<const uint8_t *>(str.data()), str.size());
		if (error.failed()) {
			return error;
		}
		return noError();
	}
};

template <> struct JsonEncodeImpl<MessagePrimitive<bool>> {
	static Error encode(typename MessagePrimitive<bool>::Reader data,
						JsonCodec::WriteContext &buffer) {
		std::string str = data.get() ? "true" : "false";
		Error error = buffer.push(
			*reinterpret_cast<const uint8_t *>(str.data()), str.size());
		if (error.failed()) {
			return error;
		}
		return noError();
	}
};

template <typename... T> struct JsonEncodeImpl<MessageList<T...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(T), Error>::type
	encodeMembers(typename MessageList<T...>::Reader data,
				  JsonCodec::WriteContext &buffer) {
		(void)data;
		(void)buffer;
		return noError();
	}
	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(T), Error>::type
		encodeMembers(typename MessageList<T...>::Reader data,
					  JsonCodec::WriteContext &buffer) {
		if (data.template get<i>().isSetExplicitly()) {
			{
				Error error =
					JsonEncodeImpl<typename ParameterPackType<i, T...>::type>::
						encode(data.template get<i>(), buffer);
				if (error.failed()) {
					return error;
				}
			}
		} else {
			{
				std::string_view str = "null";
				Error error = buffer.push(
					*reinterpret_cast<const uint8_t *>(str.data()), str.size());
				if (error.failed()) {
					return error;
				}
			}
		}
		if constexpr ((i + 1u) < sizeof...(T)) {
			Error error = buffer.push(',');
			if (error.failed()) {
				return error;
			}
		}
		{
			Error error =
				JsonEncodeImpl<MessageList<T...>>::encodeMembers<i + 1>(data,
																		buffer);
			if (error.failed()) {
				return error;
			}
		}
		return noError();
	}

	static Error encode(typename MessageList<T...>::Reader data,
						JsonCodec::WriteContext &buffer) {
		Error error = buffer.push('[');
		if (error.failed()) {
			return error;
		}
		error =
			JsonEncodeImpl<MessageList<T...>>::encodeMembers<0>(data, buffer);
		if (error.failed()) {
			return error;
		}
		error = buffer.push(']');
		if (error.failed()) {
			return error;
		}
		return noError();
	}
};

template <typename... V, typename... K>
struct JsonEncodeImpl<MessageStruct<MessageStructMember<V, K>...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), Error>::type
	encodeMembers(
		typename MessageStruct<MessageStructMember<V, K>...>::Reader data,
		JsonCodec::WriteContext &buffer) {
		(void)data;
		(void)buffer;
		return Error{};
	}
	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), Error>::type encodeMembers(
			typename MessageStruct<MessageStructMember<V, K>...>::Reader data,
			JsonCodec::WriteContext &buffer) {
		{
			Error error = buffer.push('\"');
			if (error.failed()) {
				return error;
			}
			std::string_view view = ParameterPackType<i, K...>::type::view();
			error = buffer.push(*reinterpret_cast<const uint8_t *>(view.data()),
								view.size());
			if (error.failed()) {
				return error;
			}
			error = buffer.push('\"');
			if (error.failed()) {
				return error;
			}
			error = buffer.push(':');
			if (error.failed()) {
				return error;
			}
		}
		if (data.template get<i>().isSetExplicitly()) {
			Error error =
				JsonEncodeImpl<typename ParameterPackType<i, V...>::type>::
					encode(data.template get<i>(), buffer);
			if (error.failed()) {
				return error;
			}
		} else {
			std::string_view str = "null";
			Error error = buffer.push(
				*reinterpret_cast<const uint8_t *>(str.data()), str.size());
			if (error.failed()) {
				return error;
			}
		}
		if constexpr ((i + 1u) < sizeof...(V)) {
			Error error = buffer.push(',');
			if (error.failed()) {
				return error;
			}
		}
		{
			Error error =
				JsonEncodeImpl<MessageStruct<MessageStructMember<V, K>...>>::
					encodeMembers<i + 1>(data, buffer);
			if (error.failed()) {
				return error;
			}
		}
		return noError();
	}

	static Error
	encode(typename MessageStruct<MessageStructMember<V, K>...>::Reader data,
		   JsonCodec::WriteContext &buffer) {
		Error error = buffer.push('{');
		if (error.failed()) {
			return error;
		}
		error = JsonEncodeImpl<MessageStruct<MessageStructMember<V, K>...>>::
			encodeMembers<0>(data, buffer);
		if (error.failed()) {
			return error;
		}
		error = buffer.push('}');
		if (error.failed()) {
			return error;
		}
		return noError();
	}
};

template <typename... V, typename... K>
struct JsonEncodeImpl<MessageUnion<MessageUnionMember<V, K>...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), Error>::type encodeMember(
		typename MessageUnion<MessageUnionMember<V, K>...>::Reader data,
		JsonCodec::WriteContext &buffer) {
		(void)data;
		(void)buffer;
		return noError();
	}
	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), Error>::type encodeMember(
			typename MessageUnion<MessageUnionMember<V, K>...>::Reader reader,
			JsonCodec::WriteContext &buffer) {
		/// @todo only encode if alternative is set, skip in other cases
		/// use holds_alternative

		if (reader.template holdsAlternative<
				typename ParameterPackType<i, K...>::type>()) {
			{
				Error error = buffer.push('{');
				if (error.failed()) {
					return error;
				}
			}
			{
				Error error = buffer.push('\"');
				if (error.failed()) {
					return error;
				}
				std::string_view view =
					ParameterPackType<i, K...>::type::view();
				error =
					buffer.push(*reinterpret_cast<const uint8_t *>(view.data()),
								view.size());
				if (error.failed()) {
					return error;
				}
				error = buffer.push('\"');
				if (error.failed()) {
					return error;
				}
				error = buffer.push(':');
				if (error.failed()) {
					return error;
				}
			}

			Error error =
				JsonEncodeImpl<typename ParameterPackType<i, V...>::type>::
					encode(reader.template get<i>(), buffer);
			if (error.failed()) {
				return error;
			}
			{
				Error error = buffer.push('}');
				if (error.failed()) {
					return error;
				}
			}
			return noError();
		}

		Error error =
			JsonEncodeImpl<MessageUnion<MessageUnionMember<V, K>...>>::
				encodeMember<i + 1>(reader, buffer);
		if (error.failed()) {
			return error;
		}

		return noError();
	}

	static Error
	encode(typename MessageUnion<MessageUnionMember<V, K>...>::Reader reader,
		   JsonCodec::WriteContext &buffer) {
		return encodeMember<0>(reader, buffer);
	}
};

/*
 * For JSON decoding we need a dynamic layer where we can query information from
 */
template <typename T> struct JsonDecodeImpl;

template <typename T> struct JsonDecodeImpl<MessagePrimitive<T>> {
	// static void decode(BufferView view, typename
	// MessagePrimitive<T>::Builder){}
	static Error decode(typename MessagePrimitive<T>::Builder,
						DynamicMessage::DynamicReader) {

		// This is also a valid null implementation :)
		return noError();
	}
};
template <> struct JsonDecodeImpl<MessagePrimitive<bool>> {
	// static void decode(BufferView view, typename
	// MessagePrimitive<T>::Builder){}
	static Error decode(typename MessagePrimitive<bool>::Builder data,
						DynamicMessage::DynamicReader reader) {
		if (reader.type() != DynamicMessage::Type::Bool) {
			return criticalError("Not a boolean");
		}
		DynamicMessageBool::Reader b_reader = reader.as<DynamicMessageBool>();
		data.set(b_reader.get());
		return noError();
	}
};
template <> struct JsonDecodeImpl<MessagePrimitive<int64_t>> {
	// static void decode(BufferView view, typename
	// MessagePrimitive<T>::Builder){}
	static Error decode(typename MessagePrimitive<int64_t>::Builder data,
						DynamicMessage::DynamicReader reader) {
		if (reader.type() != DynamicMessage::Type::Signed) {
			return criticalError("Not an integer");
		}
		DynamicMessageSigned::Reader s_reader =
			reader.as<DynamicMessageSigned>();
		data.set(s_reader.get());
		return noError();
	}
};
template <> struct JsonDecodeImpl<MessagePrimitive<uint32_t>> {
	// static void decode(BufferView view, typename
	// MessagePrimitive<T>::Builder){}
	static Error decode(typename MessagePrimitive<uint32_t>::Builder builder,
						DynamicMessage::DynamicReader reader) {
		if (reader.type() != DynamicMessage::Type::Signed) {
			return criticalError("Not an integer");
		}
		DynamicMessageSigned::Reader s_reader =
			reader.as<DynamicMessageSigned>();
		int64_t val = s_reader.get();
		if (val < 0) {
			return criticalError("Not an unsigned integer");
		}
		builder.set(static_cast<uint32_t>(val));
		return noError();
	}
};
template <> struct JsonDecodeImpl<MessagePrimitive<int32_t>> {
	// static void decode(BufferView view, typename
	// MessagePrimitive<T>::Builder){}
	static Error decode(typename MessagePrimitive<int32_t>::Builder data,
						DynamicMessage::DynamicReader reader) {
		if (reader.type() != DynamicMessage::Type::Signed) {
			return criticalError("Not an integer");
		}
		DynamicMessageSigned::Reader s_reader =
			reader.as<DynamicMessageSigned>();
		int64_t val = s_reader.get();
		data.set(static_cast<int32_t>(val));
		return noError();
	}
};

template <> struct JsonDecodeImpl<MessagePrimitive<std::string>> {
	static Error decode(typename MessagePrimitive<std::string>::Builder builder,
						DynamicMessage::DynamicReader reader) {
		if (reader.type() != DynamicMessage::Type::String) {
			return criticalError("Not a string");
		}
		DynamicMessageString::Reader s_reader =
			reader.as<DynamicMessageString>();
		builder.set(s_reader.get());

		return noError();
	}
};

template <typename... T> struct JsonDecodeImpl<MessageList<T...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(T), Error>::type
	decodeMembers(typename MessageList<T...>::Builder builder,
				  DynamicMessageList::Reader reader) {
		(void)builder;
		(void)reader;
		return noError();
	}

	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(T), Error>::type
		decodeMembers(typename MessageList<T...>::Builder builder,
					  DynamicMessageList::Reader reader) {

		DynamicMessage::DynamicReader member_reader = reader.get(i);

		{
			Error error =
				JsonDecodeImpl<typename ParameterPackType<i, T...>::type>::
					decode(builder.template init<i>(), member_reader);

			if (error.failed()) {
				return error;
			}
		}
		{
			Error error =
				JsonDecodeImpl<MessageList<T...>>::decodeMembers<i + 1>(builder,
																		reader);
			if (error.failed()) {
				return error;
			}
		}
		return noError();
	}

	static Error decode(typename MessageList<T...>::Builder builder,
						DynamicMessage::DynamicReader reader) {
		if (reader.type() != DynamicMessage::Type::List) {
			return criticalError("Not a list");
		}

		Error error = JsonDecodeImpl<MessageList<T...>>::decodeMembers<0>(
			builder, reader.as<DynamicMessageList>());
		if (error.failed()) {
			return error;
		}
		return noError();
	}
};

template <typename... V, typename... K>
struct JsonDecodeImpl<MessageStruct<MessageStructMember<V, K>...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), Error>::type
	decodeMembers(typename MessageStruct<MessageStructMember<V, K>...>::Builder,
				  DynamicMessageStruct::Reader reader) {
		(void)reader;
		return noError();
	}
	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), Error>::type decodeMembers(
			typename MessageStruct<MessageStructMember<V, K>...>::Builder
				builder,
			DynamicMessageStruct::Reader reader) {
		DynamicMessage::DynamicReader member_reader =
			reader.get(ParameterPackType<i, K...>::type::view());
		{
			Error error =
				JsonDecodeImpl<typename ParameterPackType<i, V...>::type>::
					decode(builder.template init<i>(), member_reader);
			if (error.failed()) {
				return error;
			}
		}
		{
			Error error =
				JsonDecodeImpl<MessageStruct<MessageStructMember<V, K>...>>::
					decodeMembers<i + 1>(builder, reader);
			if (error.failed()) {
				return error;
			}
		}
		return noError();
	}
	static Error decode(
		typename MessageStruct<MessageStructMember<V, K>...>::Builder builder,
		DynamicMessage::DynamicReader reader) {
		if (reader.type() != DynamicMessage::Type::Struct) {
			return criticalError("Not a struct");
		}
		Error error =
			JsonDecodeImpl<MessageStruct<MessageStructMember<V, K>...>>::
				decodeMembers<0>(builder, reader.as<DynamicMessageStruct>());
		if (error.failed()) {
			return error;
		}
		return noError();
	}
};

bool JsonCodec::isWhitespace(int8_t letter) {
	return letter == '\t' || letter == ' ' || letter == '\r' || letter == '\n';
}

void JsonCodec::skipWhitespace(ReadContext &ctx) {
	while (ctx.readLength() > 0 && isWhitespace(ctx.read())) {
		ctx.readAdvance(1);
	}
}

Error JsonCodec::decodeBool(DynamicMessageBool::Builder message,
							ReadContext &buffer) {
	assert((buffer.read() == 'T') || (buffer.read() == 't') ||
		   (buffer.read() == 'F') || (buffer.read() == 'f'));

	bool is_true = buffer.read() == 'T' || buffer.read() == 't';
	buffer.readAdvance(1);

	if (is_true) {
		std::array<char, 3> check = {'r', 'u', 'e'};
		for (size_t i = 0; buffer.readLength() > 0 && i < 3; ++i) {
			if (buffer.read() != check[i]) {
				return criticalError("Assumed true value, but it is invalid");
			}
			buffer.readAdvance(1);
		}
	} else {
		std::array<char, 4> check = {'a', 'l', 's', 'e'};
		for (size_t i = 0; buffer.readLength() > 0 && i < 4; ++i) {
			if (buffer.read() != check[i]) {
				return criticalError("Assumed false value, but it is invalid");
			}
			buffer.readAdvance(1);
		}
	}

	message.set(is_true);

	return noError();
}

// Not yet clear if double or integer
Error JsonCodec::decodeNumber(Own<DynamicMessage> &message,
							  ReadContext &buffer) {
	assert((buffer.read() >= '0' && buffer.read() <= '9') ||
		   buffer.read() == '+' || buffer.read() == '-');
	size_t offset = 0;

	if (buffer.read() == '-') {
		++offset;
	} else if (buffer.read() == '+') {
		return criticalError("Not a valid number with +");
	}
	if (offset >= buffer.readLength()) {
		return recoverableError("Buffer too short");
	}
	bool integer = true;
	if (buffer.read(offset) >= '1' && buffer.read(offset) <= '9') {
		++offset;

		if (offset >= buffer.readLength()) {
			return recoverableError("Buffer too short");
		}

		while (1) {
			if (buffer.read(offset) >= '0' && buffer.read(offset) <= '9') {
				++offset;

				if (offset >= buffer.readLength()) {
					return recoverableError("Buffer too short");
				}
				continue;
			}
			break;
		}
	} else if (buffer.read(offset) == '0') {
		++offset;
	} else {
		return criticalError("Not a JSON number");
	}
	if (offset >= buffer.readLength()) {
		return recoverableError("Buffer too short");
	}
	if (buffer.read(offset) == '.') {
		integer = false;
		++offset;

		if (offset >= buffer.readLength()) {
			return recoverableError("Buffer too short");
		}

		size_t partial_start = offset;

		while (1) {
			if (buffer.read(offset) >= '0' && buffer.read(offset) <= '9') {
				++offset;

				if (offset >= buffer.readLength()) {
					return recoverableError("Buffer too short");
				}
				continue;
			}
			break;
		}

		if (offset == partial_start) {
			return criticalError("No numbers after '.'");
		}
	}
	if (buffer.read(offset) == 'e' || buffer.read(offset) == 'E') {
		integer = false;
		++offset;

		if (offset >= buffer.readLength()) {
			return recoverableError("Buffer too short");
		}

		if (buffer.read(offset) == '+' || buffer.read(offset) == '-') {
			++offset;
			if (offset >= buffer.readLength()) {
				return recoverableError("Buffer too short");
			}
		}

		size_t exp_start = offset;

		while (1) {
			if (buffer.read(offset) >= '0' && buffer.read(offset) <= '9') {
				++offset;

				if (offset >= buffer.readLength()) {
					return recoverableError("Buffer too short");
				}
				continue;
			}
			break;
		}
		if (offset == exp_start) {
			return criticalError("No numbers after exponent token");
		}
	}

	if (offset >= buffer.readLength()) {
		return recoverableError("Buffer too short");
	}

	std::string_view number_view{reinterpret_cast<char *>(&buffer.read()),
								 offset};
	if (integer) {
		int64_t result;
		auto fc_result =
			std::from_chars(number_view.data(),
							number_view.data() + number_view.size(), result);
		if (fc_result.ec != std::errc{}) {
			return criticalError("Not an integer");
		}
		auto int_msg = std::make_unique<DynamicMessageSigned>();
		DynamicMessageSigned::Builder builder{*int_msg};
		builder.set(result);
		message = std::move(int_msg);
	} else {
		std::string number_copy{number_view};
		double result;
		try {
			result = std::stod(number_copy);
		} catch (std::exception &e) {
			return criticalError("Not a double");
		}
		//
		auto dbl_msg = std::make_unique<DynamicMessageDouble>();
		DynamicMessageDouble::Builder builder{*dbl_msg};
		builder.set(result);
		message = std::move(dbl_msg);
	}

	buffer.readAdvance(offset);
	skipWhitespace(buffer);
	return noError();
}

Error JsonCodec::decodeNull(ReadContext &buffer) {
	assert(buffer.read() == 'N' || buffer.read() == 'n');
	buffer.readAdvance(1);

	std::array<char, 3> check = {'u', 'l', 'l'};
	for (size_t i = 0; buffer.readLength() > 0 && i < 3; ++i) {
		if (buffer.read() != check[i]) {
			return criticalError("Assumed null value, but it is invalid");
		}
		buffer.readAdvance(1);
	}

	return noError();
}

Error JsonCodec::decodeValue(Own<DynamicMessage> &message,
							 ReadContext &buffer) {
	skipWhitespace(buffer);
	if (buffer.readLength() == 0) {
		return recoverableError("Buffer too short");
	}

	switch (buffer.read()) {
	case '"': {
		std::string str;
		Error error = decodeRawString(str, buffer);
		if (error.failed()) {
			return error;
		}
		Own<DynamicMessageString> msg_string =
			std::make_unique<DynamicMessageString>();
		DynamicMessageString::Builder builder{*msg_string};
		builder.set(std::move(str));
		message = std::move(msg_string);
	} break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '+':
	case '-': {
		Error error = decodeNumber(message, buffer);
		if (error.failed()) {
			return error;
		}
	} break;
	case 't':
	case 'T':
	case 'f':
	case 'F': {
		Own<DynamicMessageBool> msg_bool =
			std::make_unique<DynamicMessageBool>();
		decodeBool(DynamicMessageBool::Builder{*msg_bool}, buffer);
		message = std::move(msg_bool);
	} break;
	case '{': {
		Own<DynamicMessageStruct> msg_struct =
			std::make_unique<DynamicMessageStruct>();
		Error error =
			decodeStruct(DynamicMessageStruct::Builder{*msg_struct}, buffer);
		if (error.failed()) {
			return error;
		}
		message = std::move(msg_struct);
	} break;
	case '[': {
		Own<DynamicMessageList> msg_list =
			std::make_unique<DynamicMessageList>();
		decodeList(DynamicMessageList::Builder{*msg_list}, buffer);
		message = std::move(msg_list);
	} break;
	case 'n':
	case 'N': {
		Own<DynamicMessageNull> msg_null =
			std::make_unique<DynamicMessageNull>();
		decodeNull(buffer);
		message = std::move(msg_null);
	} break;
	default: {
		return criticalError("Cannot identify next JSON value");
	}
	}

	skipWhitespace(buffer);
	return noError();
}

Error JsonCodec::decodeRawString(std::string &raw, ReadContext &buffer) {
	assert(buffer.read() == '"');
	buffer.readAdvance(1);
	std::stringstream iss;
	bool string_done = false;
	while (!string_done) {
		if (buffer.readLength() == 0) {
			return recoverableError("Buffer too short");
		}
		switch (buffer.read()) {
		case '\\':
			buffer.readAdvance(1);
			if (buffer.readLength() == 0) {
				return recoverableError("Buffer too short");
			}
			switch (buffer.read()) {
			case '\\':
			case '/':
			case '"':
				iss << buffer.read();
				break;
			case 'b':
				iss << '\b';
				break;
			case 'f':
				iss << '\f';
				break;
			case 'n':
				iss << '\n';
				break;
			case 'r':
				iss << '\r';
				break;
			case 't':
				iss << '\t';
				break;
			case 'u': {
				buffer.readAdvance(1);
				if (buffer.readLength() < 4) {
					return recoverableError("Broken unicode or short buffer");
				}
				/// @todo correct unicode handling
				iss << '?'; // dummy line
				iss << '?';
				iss << '?';
				iss << '?';
				// There is always a skip at the end so here we skip 3
				// instead of 4 bytes
				buffer.readAdvance(3);
			} break;
			}
			break;
		case '"':
			string_done = true;
			break;
		default:
			iss << buffer.read();
			break;
		}
		buffer.readAdvance(1);
	}
	raw = iss.str();
	return noError();
}

Error JsonCodec::decodeList(DynamicMessageList::Builder builder,
							ReadContext &buffer) {
	assert(buffer.read() == '[');
	buffer.readAdvance(1);
	skipWhitespace(buffer);
	if (buffer.readLength() == 0) {
		return recoverableError("Buffer too short");
	}

	while (buffer.read() != ']') {

		Own<DynamicMessage> message = nullptr;
		{
			Error error = decodeValue(message, buffer);
			if (error.failed()) {
				return error;
			}
		}
		builder.push(std::move(message));
		if (buffer.readLength() == 0) {
			return recoverableError("Buffer too short");
		}

		switch (buffer.read()) {
		case ']':
			break;
		case ',':
			buffer.readAdvance(1);
			skipWhitespace(buffer);
			if (buffer.readLength() == 0) {
				return recoverableError("Buffer too short");
			}
			break;
		default:
			return criticalError("Not a JSON Object");
		}
	}
	buffer.readAdvance(1);
	return noError();
}

Error JsonCodec::decodeStruct(DynamicMessageStruct::Builder message,
							  ReadContext &buffer) {
	assert(buffer.read() == '{');
	buffer.readAdvance(1);
	skipWhitespace(buffer);
	if (buffer.readLength() == 0) {
		return recoverableError("Buffer too short");
	}

	while (buffer.read() != '}') {
		if (buffer.read() == '"') {
			std::string key_string;
			{
				Error error = decodeRawString(key_string, buffer);
				if (error.failed()) {
					return error;
				}
			}
			skipWhitespace(buffer);
			if (buffer.readLength() == 0) {
				return recoverableError("Buffer too short");
			}
			if (buffer.read() != ':') {
				return criticalError("Expecting a ':' token");
			}
			buffer.readAdvance(1);
			Own<DynamicMessage> msg = nullptr;
			{
				Error error = decodeValue(msg, buffer);
				if (error.failed()) {
					return error;
				}
			}
			message.init(key_string, std::move(msg));
			if (buffer.readLength() == 0) {
				return recoverableError("Buffer too short");
			}

			switch (buffer.read()) {
			case '}':
				break;
			case ',':
				buffer.readAdvance(1);
				skipWhitespace(buffer);
				if (buffer.readLength() == 0) {
					return recoverableError("Buffer too short");
				}
				break;
			default:
				return criticalError("Not a JSON Object");
			}
		} else {
			return criticalError("Not a JSON Object");
		}
	}
	buffer.readAdvance(1);
	return noError();
}

ErrorOr<Own<DynamicMessage>> JsonCodec::decodeDynamic(ReadContext &buffer) {
	skipWhitespace(buffer);
	if (buffer.readLength() == 0) {
		return recoverableError("Buffer too short");
	}
	if (buffer.read() == '{') {

		Own<DynamicMessageStruct> message =
			std::make_unique<DynamicMessageStruct>();
		Error error =
			decodeStruct(DynamicMessageStruct::Builder{*message}, buffer);
		if (error.failed()) {
			return error;
		}
		skipWhitespace(buffer);

		return Own<DynamicMessage>{std::move(message)};
	} else if (buffer.read() == '[') {

		Own<DynamicMessageList> message =
			std::make_unique<DynamicMessageList>();
		Error error = decodeList(*message, buffer);
		if (error.failed()) {
			return error;
		}
		skipWhitespace(buffer);

		return Own<DynamicMessage>{std::move(message)};
	} else {
		return criticalError("Not a JSON Object");
	}
}

template <typename T>
Error JsonCodec::encode(typename T::Reader reader, Buffer &buffer) {
	WriteContext context{buffer};
	Error error = JsonEncodeImpl<T>::encode(reader, context);
	if (error.failed()) {
		return error;
	}

	buffer.writeAdvance(context.offset);

	return error;
}

template <typename T>
Error JsonCodec::decode(typename T::Builder builder, Buffer &buffer,
						const Limits &limits) {

	ReadContext context{buffer, limits};

	ErrorOr<Own<DynamicMessage>> error_or_message = decodeDynamic(context);
	if (error_or_message.isError()) {
		return error_or_message.error();
	}

	buffer.readAdvance(context.offset);

	Own<DynamicMessage> message = std::move(error_or_message.value());
	if (!message) {
		return criticalError("No message object created");
	}
	if (message->type() == DynamicMessage::Type::Null) {
		return criticalError("Can't decode to json");
	}
	DynamicMessage::DynamicReader reader{*message};
	Error static_error = JsonDecodeImpl<T>::decode(builder, reader);
	return static_error;
}

} // namespace gin
