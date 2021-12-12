#pragma once

#include <cstdint>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

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
 * Description which type to use happens through the use of the schema classes
 * in schema.h The message classes are wrapper classes which store the data according
 * to the specified container class.  
 *
 * The reader and builder classe exist to create some clarity while implementing parsers.
 * Also minor guarantess are provided if the message is used as a template parameter.
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
				  "Container should have same Schema as Message");

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

		/*
		 * Initialize a member by index
		 */
		template <size_t i> typename Container::ElementType<i>::Builder init() {
			return typename Container::ElementType<i>::Builder{
				message.container.get<i>()};
		}

		/*
		 * Initialize a member by their name
		 * This is the preferred method for schema::Struct messages
		 */
		template <StringLiteral Literal>
		typename Container::ElementType<MessageParameterPackIndex<Literal,Keys...>::Value>::Builder init() {
			constexpr size_t i =
				MessageParameterPackIndex<Literal, Keys...>::Value;

			return init<i>();
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
		template <size_t i> typename Container::ElementType<i>::Reader get() {
			return typename Container::ElementType<i>::Reader{
				message.container.get<i>()};
		}

		/*
		 * Get member by name
		 * This is the preferred method for schema::Struct messages
		 */
		template <StringLiteral Literal>
		typename Container::ElementType<MessageParameterPackIndex<Literal,Keys...>::Value>::Reader get() {
			// The index of the first match
			constexpr size_t i =
				MessageParameterPackIndex<Literal, Keys...>::Value;

			return get<i>();
		}
	};
};

/*
 * Union message class. Wrapper object 
 */
template<class... V, StringLiteral... Keys, class Container>
class Message<schema::Union<schema::NamedMember<V,Keys>...>, Container> final : public MessageBase {
private:
	using SchemaType = schema::Union<schema::NamedMember<V, Keys>...>;
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
		MessageType & message;
	public:
		Builder(MessageType& msg):message{msg}{}

		Reader asReader(){return Reader{message};}
		
		template<size_t i>
		typename Container::ElementType<i>::Builder init(){
			return typename Container::ElementType<i>::Builder{message.container.get<i>()};
		}
	};

	class Reader {
	private:
		MessageType& message;
	public:
		Reader(MessageType& msg):message{msg}{}

		Builder asBuilder(){return Builder{message};}

		template<size_t i> typename Container::ElementType<i>::Reader get(){
			return typename Container::ElementType<i>::Reader{message.container.get<i>()};
		}

		template<StringLiteral Literal>
		constexpr size_t index() const noexcept {
			return MessageParameterPackIndex<Literal, Keys...>::Value;
		}
	};
};

/*
 * Array message class. Wrapper around an array schema element
 * @todo Array class needs either a resize function or each message class has an individual call for Array children
 */
/*
template<class T, class Container>
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
		MessageType & message;
	public:
		Builder(MessageType& msg):message{msg}{}

		Reader asReader(){return Reader{message};}

		template<size_t i>
		typename Container::MessageType::Builder init(){
			return typename Container::MessageType::Builder{message.container.get<i>()};
		}
	};
	
	class Reader {
	private:
		MessageType& message;
	public:
		Reader(MessageType& msg):message{msg}{}

		Builder asBuilder(){return Builder{message};}

		template<size_t i>
		typename Container::MessageType::Reader get(){
			return typename Container::MessageType::Reader{message.container.get<i>()};
		}
	};
};
*/

/*
 * Tuple message class. Wrapper around a tuple schema
 */
template<class... T>
class Message<schema::Tuple<T...>, Container> final : public MessageBase {
private:
	using SchemaType = schema::Tuple<T...>;
	using MessageType = Message<SchemaType, Container>;
	
	Container container;

	static_assert(std::is_same_v<SchemaType, typename Container::SchemaType>,
				  "Container should have same Schema as Message");

	friend class Builder;
	friend class Reader;
public:
	class Reader;
	class Builder {
		MessageType & message;
	public:
		Builder(MessageType& msg):message{msg}{}

		Reader asReader(){return Reader{message};}

		template<size_t i>
		typename Container::MessageType::Builder init(){
			return typename Container::MessageType::Builder{message.container.get<i>()};
		}
	};
	class Reader {
	private:
		MessageType& message;
	public:
		Reader(MessageType& msg):message{msg}{}

		Builder asBuilder(){return Builder{message};}

		template<size_t i>
		typename Container::MessageType::Reader get(){
			return typename Container::MessageType::Reader{message.container.get<i>()};
		}
	};
};

/*
 * Primitive type (float, double, uint8_t, uint16_t, uint32_t, uint64_t, int8_t, int16_t, int32_t, int64_t) message class
 */
template <class T, size_t N, class Container>
class Message<schema::Primitive<T, N>, Container> final : public MessageBase {
private:
	using SchemaType = schema::Primitive<T, N>;
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
		Builder(MessageType & msg) : message{msg} {}

		Reader asReader() { return Reader{message}; }

		void set(const T &value) { message.container.set(value); }
	};

	class Reader {
	private:
		MessageType &message;

	public:
		Reader(Message & msg) : message{msg} {}

		Builder asBuilder() { return Builder{message}; }

		const T &get() const { return message.container.get(); }
	};
};

template <class Container>
class Message<schema::String, Container> final : public MessageBase {
private:
	using SchemaType = schema::String;
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
		Builder(MessageType & msg) : message{msg} {}

		Reader asReader() { return Reader{message}; }

		void set(const std::string &str) { message.container.set(str); }
	};

	class Reader {
	private:
		MessageType &message;

	public:
		Reader(MessageType & msg) : message{msg} {}

		Builder asBuilder() { return Builder{message}; }

		std::string_view get() { return message.container.get(); }
	}
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
