#pragma once

#include <cstdint>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include <cassert>

#include "common.h"

#include "message_container.h"
#include "schema.h"
#include "string_literal.h"

namespace saw {
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
 * Description which type to use happens through the use of the schema classes
 * in schema.h The message classes are wrapper classes which store the data
 * according to the specified container class.
 *
 * The reader and builder classe exist to create some clarity while implementing
 * parsers. Also minor guarantess are provided if the message is used as a
 * template parameter.
 */

/*
 * Struct Message
 */
template <class... V, StringLiteral... Keys, class Container>
class Message<schema::Struct<schema::NamedMember<V, Keys>...>, Container> final
	: public MessageBase {
private:
	using SchemaType = schema::Struct<schema::NamedMember<V, Keys>...>;
	using MessageType = Message<SchemaType, Container>;
	Container container;

	static_assert(std::is_same_v<SchemaType, typename Container::SchemaType>,
				  "Container should have same the schema as Message");

	friend class Builder;
	friend class Reader;

public:
	class Reader;
	class Builder {
	private:
		MessageType &message;

	public:
		Builder(MessageType &msg) : message{msg} {}

		Reader asReader() { return Reader{message}; }

		/*
		 * Initialize a member by index
		 */
		template <size_t i>
		typename std::enable_if<
			!SchemaIsArray<
				typename MessageParameterPackType<i, V...>::Type>::Value,
			typename Container::template ElementType<i>::Builder>::type
		init() {
			return typename Container::template ElementType<i>::Builder{
				message.container.template get<i>()};
		}

		/*
		 * Initialize a member by their name
		 * This is the preferred method for schema::Struct messages
		 */
		template <StringLiteral Literal>
		typename std::enable_if<
			!SchemaIsArray<typename MessageParameterPackType<
				MessageParameterKeyPackIndex<Literal, Keys...>::Value,
				V...>::Type>::Value,
			typename Container::template ElementType<
				MessageParameterKeyPackIndex<Literal,
											 Keys...>::Value>::Builder>::type
		init() {
			constexpr size_t i =
				MessageParameterKeyPackIndex<Literal, Keys...>::Value;

			return init<i>();
		}

		template <size_t i>
		typename std::enable_if<
			SchemaIsArray<
				typename MessageParameterPackType<i, V...>::Type>::Value,
			typename Container::template ElementType<i>::Builder>::type
		init(size_t size = 0) {
			auto array_builder =
				typename Container::template ElementType<i>::Builder{
					message.container.template get<i>(), size};
			return array_builder;
		}

		/*
		 * Version for array schema type
		 */
		template <StringLiteral Literal>
		typename std::enable_if<
			SchemaIsArray<typename MessageParameterPackType<
				MessageParameterKeyPackIndex<Literal, Keys...>::Value,
				V...>::Type>::Value,
			typename Container::template ElementType<
				MessageParameterKeyPackIndex<Literal,
											 Keys...>::Value>::Builder>::type
		init(size_t size) {
			constexpr size_t i =
				MessageParameterKeyPackIndex<Literal, Keys...>::Value;

			return init<i>(size);
		}
	};

	class Reader {
	private:
		MessageType &message;

	public:
		Reader(MessageType &msg) : message{msg} {}

		Builder asBuilder() { return Builder{message}; }

		/*
		 * Get member by index
		 */
		template <size_t i>
		typename Container::template ElementType<i>::Reader get() {
			return typename Container::template ElementType<i>::Reader{
				message.container.template get<i>()};
		}

		/*
		 * Get member by name
		 * This is the preferred method for schema::Struct messages
		 */
		template <StringLiteral Literal>
		typename Container::template ElementType<
			MessageParameterKeyPackIndex<Literal, Keys...>::Value>::Reader
		get() {
			// The index of the first match
			constexpr size_t i =
				MessageParameterKeyPackIndex<Literal, Keys...>::Value;

			return get<i>();
		}
	};

	Builder build() { return Builder{*this}; }

	Reader read() { return Reader{*this}; }
};

/*
 * Union message class. Wrapper object
 */
template <class... V, StringLiteral... Keys, class Container>
class Message<schema::Union<schema::NamedMember<V, Keys>...>, Container> final
	: public MessageBase {
private:
	using SchemaType = schema::Union<schema::NamedMember<V, Keys>...>;
	using MessageType = Message<SchemaType, Container>;

	Container container;

	static_assert(std::is_same_v<SchemaType, typename Container::SchemaType>,
				  "Container should have same the schema as Message");

	friend class Builder;
	friend class Reader;

public:
	class Reader;
	class Builder {
	private:
		MessageType &message;

	public:
		Builder(MessageType &msg) : message{msg} {}

		Reader asReader() { return Reader{message}; }

		template <size_t i>
		typename std::enable_if<
			!SchemaIsArray<
				typename MessageParameterPackType<i, V...>::Type>::Value,
			typename Container::template ElementType<i>::Builder>::type

		init() {
			return typename Container::template ElementType<i>::Builder{
				message.container.template get<i>()};
		}

		template <StringLiteral Literal>
		typename std::enable_if<
			!SchemaIsArray<typename MessageParameterPackType<
				MessageParameterKeyPackIndex<Literal, Keys...>::Value,
				V...>::Type>::Value,
			typename Container::template ElementType<
				MessageParameterKeyPackIndex<Literal,
											 Keys...>::Value>::Builder>::type
		init() {
			constexpr size_t i =
				MessageParameterKeyPackIndex<Literal, Keys...>::Value;

			return init<i>();
		}

		/*
		 * If Schema is Array
		 */
		template <size_t i>
		typename std::enable_if<
			SchemaIsArray<
				typename MessageParameterPackType<i, V...>::Type>::Value,
			typename Container::template ElementType<i>::Builder>::type
		init(size_t size = 0) {
			return typename Container::template ElementType<i>::Builder{
				message.container.template get<i>(), size};
		}

		template <StringLiteral Literal>
		typename std::enable_if<
			SchemaIsArray<typename MessageParameterPackType<
				MessageParameterKeyPackIndex<Literal, Keys...>::Value,
				V...>::Type>::Value,
			typename Container::template ElementType<
				MessageParameterKeyPackIndex<Literal,
											 Keys...>::Value>::Builder>::type
		init(size_t size) {
			constexpr size_t i =
				MessageParameterKeyPackIndex<Literal, Keys...>::Value;

			return init<i>(size);
		}
	};

	class Reader {
	private:
		MessageType &message;

	public:
		Reader(MessageType &msg) : message{msg} {}

		Builder asBuilder() { return Builder{message}; }

		template <size_t i>
		typename Container::template ElementType<i>::Reader get() {
			return typename Container::template ElementType<i>::Reader{
				message.container.template get<i>()};
		}

		template <StringLiteral Literal>
		typename Container::template ElementType<
			MessageParameterKeyPackIndex<Literal, Keys...>::Value>::Reader
		get() {
			// The index of the first match
			constexpr size_t i =
				MessageParameterKeyPackIndex<Literal, Keys...>::Value;

			return get<i>();
		}

		template <StringLiteral Literal>
		constexpr size_t toIndex() const noexcept {
			return MessageParameterKeyPackIndex<Literal, Keys...>::Value;
		}

		size_t index() const noexcept { return message.container.index(); }

		template <StringLiteral Literal> bool hasAlternative() const {
			return index() == toIndex<Literal>();
		}
	};
};

/*
 * Array message class. Wrapper around an array schema element
 */
template <class T, class Container>
class Message<schema::Array<T>, Container> final : public MessageBase {
private:
	using SchemaType = schema::Array<T>;
	using MessageType = Message<SchemaType, Container>;

	Container container;

	static_assert(std::is_same_v<SchemaType, typename Container::SchemaType>,
				  "Container should have same Schema as Message");

	friend class Builder;
	friend class Reader;

public:
	class Reader;
	class Builder {
	private:
		MessageType &message;

	public:
		Builder(MessageType &msg, size_t size) : message{msg} {
			if (size > 0) {
				message.container.resize(size);
			}
		}

		Reader asReader() { return Reader{message}; }

		typename Container::ElementType::Builder init(size_t i) {
			return typename Container::ElementType::Builder{
				message.container.get(i)};
		}

		size_t size() const { return message.container.size(); }

		void resize(size_t size) { message.container.resize(size); }
	};

	class Reader {
	private:
		MessageType &message;

	public:
		Reader(MessageType &msg) : message{msg} {}

		Builder asBuilder() { return Builder{message, 0}; }

		typename Container::ElementType::Reader get(size_t i) {
			return typename Container::ElementType::Reader{
				message.container.get(i)};
		}

		size_t size() const { return message.container.size(); }
	};
};

/*
 * Tuple message class. Wrapper around a tuple schema
 */
template <class... T, class Container>
class Message<schema::Tuple<T...>, Container> final : public MessageBase {
private:
	using SchemaType = schema::Tuple<T...>;
	using MessageType = Message<SchemaType, Container>;

	Container container;

	static_assert(std::is_same_v<SchemaType, typename Container::SchemaType>,
				  "Container should have same the schema as Message");

	friend class Builder;
	friend class Reader;

public:
	class Reader;
	class Builder {
		MessageType &message;

	public:
		Builder(MessageType &msg) : message{msg} {}

		Reader asReader() { return Reader{message}; }

		template <size_t i>
		typename std::enable_if<
			!SchemaIsArray<
				typename MessageParameterPackType<i, T...>::Type>::Value,
			typename Container::template ElementType<i>::Builder>::type
		init() {
			return typename Container::template ElementType<i>::Builder{
				message.container.template get<i>()};
		}

		template <size_t i>
		typename std::enable_if<
			SchemaIsArray<
				typename MessageParameterPackType<i, T...>::Type>::Value,
			typename Container::template ElementType<i>::Builder>::type
		init(size_t size = 0) {
			return typename Container::template ElementType<i>::Builder{
				message.container.template get<i>(), size};
		}
	};
	class Reader {
	private:
		MessageType &message;

	public:
		Reader(MessageType &msg) : message{msg} {}

		Builder asBuilder() { return Builder{message}; }

		template <size_t i>
		typename Container::template ElementType<i>::Reader get() {
			return typename Container::template ElementType<i>::Reader{
				message.container.template get<i>()};
		}
	};
};

/*
 * Primitive type (float, double, uint8_t, uint16_t, uint32_t, uint64_t, int8_t,
 * int16_t, int32_t, int64_t) message class
 */
template <class T, size_t N, class Container>
class Message<schema::Primitive<T, N>, Container> final : public MessageBase {
private:
	using SchemaType = schema::Primitive<T, N>;
	using MessageType = Message<SchemaType, Container>;

	Container container;

	static_assert(std::is_same_v<SchemaType, typename Container::SchemaType>,
				  "Container should have same the schema as Message");

	friend class Builder;
	friend class Reader;

public:
	class Reader;
	class Builder {
	private:
		MessageType &message;

	public:
		Builder(MessageType &msg) : message{msg} {}

		Reader asReader() { return Reader{message}; }

		void set(const typename Container::ValueType &value) {
			message.container.set(value);
		}
	};

	class Reader {
	private:
		MessageType &message;

	public:
		Reader(Message &msg) : message{msg} {}

		Builder asBuilder() { return Builder{message}; }

		const typename Container::ValueType &get() const {
			return message.container.get();
		}
	};
};

template <class Container>
class Message<schema::String, Container> final : public MessageBase {
private:
	using SchemaType = schema::String;
	using MessageType = Message<SchemaType, Container>;

	Container container;

	static_assert(std::is_same_v<SchemaType, typename Container::SchemaType>,
				  "Container should have same the schema as Message");

	friend class Builder;
	friend class Reader;

public:
	class Reader;
	class Builder {
	private:
		MessageType &message;

	public:
		Builder(MessageType &msg) : message{msg} {}

		Reader asReader() { return Reader{message}; }

		void set(std::string &&str) { message.container.set(std::move(str)); }
		void set(const std::string_view str) { message.container.set(str); }
		void set(const char *str) { set(std::string_view{str}); }
	};

	class Reader {
	private:
		MessageType &message;

	public:
		Reader(MessageType &msg) : message{msg} {}

		Builder asBuilder() { return Builder{message}; }

		const std::string_view get() const { return message.container.get(); }
	};
};

template <class Schema, class Container = MessageContainer<Schema>>
class HeapMessageRoot {
private:
	Own<Message<Schema, Container>> root;

public:
	HeapMessageRoot(Own<Message<Schema, Container>> r) : root{std::move(r)} {}

	typename Message<Schema, Container>::Builder build() {
		assert(root);
		return typename Message<Schema, Container>::Builder{*root};
	}

	typename Message<Schema, Container>::Reader read() {
		assert(root);
		return typename Message<Schema, Container>::Reader{*root};
	}
};

template <class T, class Container>
class HeapMessageRoot<schema::Array<T>, Container> {
public:
	using Schema = schema::Array<T>;

private:
	Own<Message<Schema, Container>> root;

public:
	HeapMessageRoot(Own<Message<Schema, Container>> r) : root{std::move(r)} {}

	typename Message<Schema, Container>::Builder build(size_t size) {
		assert(root);
		return typename Message<Schema, Container>::Builder{*root, size};
	}

	typename Message<Schema, Container>::Reader read() {
		assert(root);
		return typename Message<Schema, Container>::Reader{*root};
	}
};

/*
 * Minor helper for creating a message root
 */
template <class Schema, class Container = MessageContainer<Schema>>
inline HeapMessageRoot<Schema, Container> heapMessageRoot() {
	Own<Message<Schema, Container>> root = heap<Message<Schema, Container>>();
	return HeapMessageRoot<Schema, Container>{std::move(root)};
}
} // namespace saw
