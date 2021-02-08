#pragma once

#include "buffer.h"
#include "message.h"
#include "message_dynamic.h"

#include "error.h"

#include <cassert>
#include <charconv>
#include <sstream>
#include <string_view>
#include <tuple>

#include <iostream>

namespace gin {
template <typename T> struct JsonEncodeImpl;

template <typename T> struct JsonEncodeImpl<MessagePrimitive<T>> {
	static Error encode(typename MessagePrimitive<T>::Reader data,
						Buffer &buffer) {
		std::string stringified = std::to_string(data.get());
		Error error =
			buffer.push(*reinterpret_cast<const uint8_t *>(stringified.data()),
						stringified.size());
		if (error.failed()) {
			return error;
		}
		return Error{};
	}
};

template <> struct JsonEncodeImpl<MessagePrimitive<std::string>> {
	static Error encode(typename MessagePrimitive<std::string>::Reader data,
						Buffer &buffer) {
		std::string str =
			std::string{"\""} + std::string{data.get()} + std::string{"\""};
		Error error = buffer.push(
			*reinterpret_cast<const uint8_t *>(str.data()), str.size());
		if (error.failed()) {
			return error;
		}
		return Error{};
	}
};

template <typename... T> struct JsonEncodeImpl<MessageList<T...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(T), Error>::type
	encodeMembers(typename MessageList<T...>::Reader data, Buffer &buffer) {
		(void)data;
		(void)buffer;
		return Error{};
	}
	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(T), Error>::type
		encodeMembers(typename MessageList<T...>::Reader data, Buffer &buffer) {
		{
			Error error =
				JsonEncodeImpl<typename ParameterPackType<i, T...>::type>::
					encode(data.template get<i>(), buffer);
			if (error.failed()) {
				return error;
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
						Buffer &buffer) {
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
		Buffer &buffer) {
		(void)data;
		(void)buffer;
		return Error{};
	}
	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), Error>::type encodeMembers(
			typename MessageStruct<MessageStructMember<V, K>...>::Reader data,
			Buffer &buffer) {
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
		{
			Error error =
				JsonEncodeImpl<typename ParameterPackType<i, V...>::type>::
					encode(data.template get<i>(), buffer);
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
		   Buffer &buffer) {
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
		Buffer &buffer) {
		(void)data;
		(void)buffer;
		return noError();
	}
	template <size_t i = 0>
		static typename std::enable_if <
		i<sizeof...(V), Error>::type encodeMember(
			typename MessageUnion<MessageUnionMember<V, K>...>::Reader reader,
			Buffer &buffer) {
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
		   Buffer &buffer) {
		return encodeMember<0>(reader, buffer);
	}
};

/*
 * For JSON decoding we need a dynamic where we can query information from
 */
template <typename T> struct JsonDecodeImpl;

template <typename T> struct JsonDecodeImpl<MessagePrimitive<T>> {
	// static void decode(BufferView view, typename
	// MessagePrimitive<T>::Builder){}
	static Error decode(typename MessagePrimitive<T>::Builder,
						DynamicMessage::DynamicReader) {
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

template <typename... V, typename... K>
struct JsonDecodeImpl<MessageStruct<MessageStructMember<V, K>...>> {
	template <size_t i = 0>
	static typename std::enable_if<i == sizeof...(V), Error>::type
	decodeMembers(typename MessageStruct<MessageStructMember<V, K>...>::Builder,
				  DynamicMessageStruct::Reader reader) {
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

class JsonCodec {
private:
	bool isWhitespace(int8_t letter) {
		return letter == '\t' || letter == ' ' || letter == '\r' ||
			   letter == '\n';
	}

	void skipWhitespace(Buffer &buffer) {
		while (buffer.readCompositeLength() > 0 &&
			   isWhitespace(buffer.read())) {
			buffer.readAdvance(1);
		}
	}

	Error decodeBool(DynamicMessageBool::Builder message, Buffer &buffer) {
		/// @todo unimplemented
		return noError();
	}

	// Not yet clear if double or integer
	Error decodeNumber(Own<DynamicMessage> &message, Buffer &buffer) {
		assert((buffer.read() >= '0' && buffer.read() <= '9') ||
			   buffer.read() == '+' || buffer.read() == '-');
		size_t offset = 0;

		if (buffer.read() == '-') {
			++offset;
		} else if (buffer.read() == '+') {
			return criticalError("Not a valid number with +");
		}
		if (offset >= buffer.readCompositeLength()) {
			return recoverableError("Buffer too short");
		}
		bool integer = true;
		if (buffer.read(offset) >= '1' && buffer.read(offset) <= '9') {
			++offset;

			if (offset >= buffer.readCompositeLength()) {
				return recoverableError("Buffer too short");
			}

			while (1) {
				if (buffer.read(offset) >= '0' && buffer.read(offset) <= '9') {
					++offset;

					if (offset >= buffer.readCompositeLength()) {
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
		if (offset >= buffer.readCompositeLength()) {
			return recoverableError("Buffer too short");
		}
		if (buffer.read(offset) == '.') {
			integer = false;
			++offset;

			if (offset >= buffer.readCompositeLength()) {
				return recoverableError("Buffer too short");
			}

			size_t partial_start = offset;

			while (1) {
				if (buffer.read(offset) >= '0' && buffer.read(offset) <= '9') {
					++offset;

					if (offset >= buffer.readCompositeLength()) {
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

			if (offset >= buffer.readCompositeLength()) {
				return recoverableError("Buffer too short");
			}

			if (buffer.read(offset) == '+' || buffer.read(offset) == '-') {
				++offset;
				if (offset >= buffer.readCompositeLength()) {
					return recoverableError("Buffer too short");
				}
			}

			size_t exp_start = offset;

			while (1) {
				if (buffer.read(offset) >= '0' && buffer.read(offset) <= '9') {
					++offset;

					if (offset >= buffer.readCompositeLength()) {
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

		if (offset >= buffer.readCompositeLength()) {
			return recoverableError("Buffer too short");
		}

		std::string_view number_view{reinterpret_cast<char *>(&buffer.read()),
									 offset};
		if (integer) {
			int64_t result;
			auto fc_result = std::from_chars(
				number_view.data(), number_view.data() + number_view.size(),
				result);
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

	Error decodeNull(Buffer &buffer) {
		/// @todo unimplemented
		return noError();
	}

	Error decodeValue(Own<DynamicMessage> &message, Buffer &buffer) {
		skipWhitespace(buffer);
		if (buffer.readCompositeLength() == 0) {
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
			Error error = decodeStruct(
				DynamicMessageStruct::Builder{*msg_struct}, buffer);
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

	Error decodeRawString(std::string &raw, Buffer &buffer) {
		assert(buffer.read() == '"');
		buffer.readAdvance(1);
		std::stringstream iss;
		bool string_done = false;
		while (!string_done) {
			if (buffer.readCompositeLength() == 0) {
				return recoverableError("Buffer too short");
			}
			switch (buffer.read()) {
			case '\\':
				buffer.readAdvance(1);
				if (buffer.readCompositeLength() == 0) {
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
					if (buffer.readCompositeLength() < 4) {
						return recoverableError(
							"Broken unicode or short buffer");
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

	Error decodeList(DynamicMessageList::Builder builder, Buffer &buffer) {
		assert(buffer.read() == '[');
		buffer.readAdvance(1);
		skipWhitespace(buffer);
		if (buffer.readCompositeLength() == 0) {
			return recoverableError("Buffer too short");
		}
		if (buffer.read() == ']') {
			return noError();
		}

		Own<DynamicMessage> message = nullptr;
		{ Error error = decodeValue(message, buffer); }

		while (buffer.read() != ']') {
		}
		/// @todo unimplemented
		return noError();
	}

	Error decodeStruct(DynamicMessageStruct::Builder message, Buffer &buffer) {
		assert(buffer.read() == '{');
		buffer.readAdvance(1);
		skipWhitespace(buffer);
		if (buffer.readCompositeLength() == 0) {
			return recoverableError("Buffer too short");
		}
		if (buffer.read() == '}') {
			buffer.readAdvance(1);
			return noError();
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
				if (buffer.readCompositeLength() == 0) {
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
				if (buffer.readCompositeLength() == 0) {
					return recoverableError("Buffer too short");
				}

				/*
				skipWhitespace(buffer);
				if(buffer.readCompositeLength() == 0){
					return recoverableError("Buffer too short");
				}
				/// @todo value decode
				skipWhitespace(buffer);
				*/
				switch (buffer.read()) {
				case '}':
					break;
				case ',':
					buffer.readAdvance(1);
					skipWhitespace(buffer);
					if (buffer.readCompositeLength() == 0) {
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

	ErrorOr<Own<DynamicMessage>> decodeDynamic(Buffer &buffer) {
		skipWhitespace(buffer);
		if (buffer.readCompositeLength() == 0) {
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

public:
	template <typename T>
	Error encode(typename T::Reader reader, Buffer &buffer) {
		return JsonEncodeImpl<T>::encode(reader, buffer);
	}

	template <typename T>
	Error decode(typename T::Builder builder, Buffer &buffer) {
		ErrorOr<Own<DynamicMessage>> error_or_message = decodeDynamic(buffer);
		if (error_or_message.isError()) {
			return error_or_message.error();
		}

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
};

} // namespace gin
