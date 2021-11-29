#pragma once

#include <cstdint>
#include <tuple>
#include <variant>
#include <vector>
#include <type_traits>

#include "common.h"

#include "schema.h"
#include "string_literal.h"

namespace gin {
class MessageBase {
protected:
	bool set_explicitly = false;

public:
	template <class T> T &as() {
		static_assert(std::is_base_of<MessageBase, T>());
		return dynamic_cast<T &>(*this);
	}

	template <class T> const T &as() const {
		static_assert(std::is_base_of<MessageBase, T>());
		return dynamic_cast<const T &>(*this);
	}
};
/*
 * Representing all message types
 * Description which type to use happens through the use of the schema classes in schema.h
 * The message classes are a container class which either contains singular types or has
 * some form of encoding in a buffer present
 */

template <class... V, StringLiteral... Keys, class Container>
class Message<schema::Struct<schema::NamedMember<V, Keys>...>, Container> final
	: public MessageBase {
private:
	using SchemaType = schema::Struct<schema::NamedMember<V,Keys>...>;
	using MessageType =
		Message<SchemaType, Container>;
	Container container;

	static_assert(std::is_same_v<SchemaType, typename Container::SchemaType>, "Container should have same Schema as Message");

	friend class Builder;
	friend class Reader;

public:
	class Reader;
	class Builder {
	private:
		MessageType &message;

	public:
		Builder(MessageType &msg) : message{msg} {}

		Reader asReader() { return Reader{MessageType & message}; }

		template<size_t i>
		typename Container::Element<i>::Builder init(){
			return typename Container::Element<i>::Builder{message.container.get<i>()};
		}

		template<StringLiteral Literal> typename Container::Element<i>::Builder init(){
			constexpr size_t i = MessageParameterPackIndex<Literal, Keys...>::Value;

			return init<i>();
		}
	};

	class Reader {
	private:
		MessageType &message;

	public:
		Reader(MessageType &msg) : message{msg} {}

		Builder asBuilder() { return Builder{MessageType & message}; }

		template<size_t i>
		typename Container::Element<i>::Reader get(){
			return typename Container::Element<i>::Reader{message.container.get<i>()};
		}

		template<StringLiteral Literal> typename Container::Element<i>::Reader get(){
			constexpr size_t i = MessageParameterPackIndex<Literal, Keys...>::Value;

			return get<i>();
		}
	};
};

template<class T, size_t N, class Container>
class Message<schema::Primitive<T,N>, Container> final : public MessageBase {
private:
	Container container;
public:

};

class MessageReader {
public:
	virtual ~MessageReader() = default;
};

class MessageBuilder {
private:
	Own<Message> root_message = nullptr;

public:
	virtual ~MessageBuilder() = default;

	template <typename MessageRoot> typename MessageRoot::Builder initRoot() {
		root_message = std::make_unique<MessageRoot>();
		MessageRoot &msg_ref = root_message->as<MessageRoot>();
		return typename MessageRoot::Builder{msg_ref};
	}
};

inline MessageBuilder messageBuilder() { return MessageBuilder{}; }
} // namespace gin
