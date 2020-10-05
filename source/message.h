#pragma once

#include <cstdint>
#include <tuple>
#include <variant>

#include "common.h"

#include "string_literal.h"

namespace gin {
class Message {
protected:
	bool set_explicitly = false;

public:
	template <typename T> T &as() {
		static_assert(std::is_base_of<Message, T>());
		return reinterpret_cast<T &>(*this);
	}

	template <typename T> const T &as() const {
		static_assert(std::is_base_of<Message, T>());
		return reinterpret_cast<const T &>(*this);
	}
};
/*
 * Representing all primitive types
 */
template <typename T> class MessagePrimitive : public Message {
private:
	T value;

	friend class Builder;
	friend class Reader;

public:
	MessagePrimitive() = default;

	class Reader;
	class Builder {
	private:
		MessagePrimitive<T> &message;

	public:
		Builder(MessagePrimitive<T> &message) : message{message} {}

		constexpr void set(const T &value) {
			message.value = value;
			message.set_explicitly = true;
		}

		Reader asReader() { return Reader{message}; }
	};

	class Reader {
	private:
		MessagePrimitive<T> &message;

	public:
		Reader(MessagePrimitive<T> &message) : message{message} {}

		const T &get() const { return message.value; }

		bool isSetExplicitly() const { return message.set_explicitly; }

		Builder asBuilder() { return Builder{message}; }
	};
};
template <> class MessagePrimitive<std::string> : public Message {
private:
	std::string value;

	friend class Builder;
	friend class Reader;

public:
	MessagePrimitive() = default;

	class Reader;
	class Builder {
	private:
		MessagePrimitive<std::string> &message;

	public:
		Builder(MessagePrimitive<std::string> &message) : message{message} {}

		void set(std::string_view value) {
			message.value = std::string{value};
			message.set_explicitly = true;
		}
		/*
		void set(std::string &&value) {
			message.value = std::move(value);
			message.set_explicitly = true;
		}
		*/

		Reader asReader() { return Reader{message}; }
	};

	class Reader {
	private:
		MessagePrimitive<std::string> &message;

	public:
		Reader(MessagePrimitive<std::string> &message) : message{message} {}

		std::string_view get() const { return std::string_view{message.value}; }

		bool isSetExplicitly() const { return message.set_explicitly; }

		Builder asBuilder() { return Builder{message}; }
	};
};

template <typename... Args> class MessageList : public Message {
private:
	using tuple_type = std::tuple<Args...>;
	tuple_type elements;
	friend class Builder;
	friend class Reader;

public:
	class Reader;
	class Builder {
	private:
		MessageList<Args...> &message;

	public:
		Builder(MessageList<Args...> &message) : message{message} {
			message.set_explicitly = true;
		}

		template <size_t i>
		constexpr typename std::tuple_element_t<i, tuple_type>::Builder init() {
			std::tuple_element_t<i, tuple_type> &msg_ref =
				std::get<i>(message.elements);
			return
				typename std::tuple_element_t<i, tuple_type>::Builder{msg_ref};
		}

		Reader asReader() { return Reader{message}; }
	};

	class Reader {
	private:
		MessageList<Args...> &message;

	public:
		Reader(MessageList<Args...> &message) : message{message} {}

		template <size_t i>
		constexpr typename std::tuple_element_t<i, tuple_type>::Reader get() {
			return std::get<i>(message.elements);
		}

		size_t size() const { return std::tuple_size<tuple_type>::value; }

		bool isSetExplicitly() const { return message.set_explicitly; }

		Builder asBuilder() { return Reader{message}; }
	};
};
template <typename T, typename K> struct MessageStructMember;

template <typename T, typename C, C... Chars>
struct MessageStructMember<T, StringLiteral<C, Chars...>> {
	T value;
};

/*
 * Helper structs which retrieve
 * the index of a parameter pack based on the index
 * or
 * a type based on the index
 * Pass N as the index for the desired type
 * or
 * a type to get the first occurence of a type index
 */
template <size_t N, typename... T> struct ParameterPackType;

template <typename T, typename... TL> struct ParameterPackType<0, T, TL...> {
	using type = T;
};

template <size_t N, typename T, typename... TL>
struct ParameterPackType<N, T, TL...> {
	using type = typename ParameterPackType<N - 1, TL...>::type;
};

template <typename T, typename... TL> struct ParameterPackIndex;

template <typename T, typename TL0, typename... TL>
struct ParameterPackIndex<T, TL0, TL...> {
	static constexpr size_t value = 1u + ParameterPackIndex<T, TL...>::value;
};

template <typename T, typename... TL> struct ParameterPackIndex<T, T, TL...> {
	static constexpr size_t value = 0u;
};

template <typename... T> class MessageStruct;

/*
 * Since no value is retrieved from the keys, I only need a value tuple
 */
template <typename... V, typename... K>
class MessageStruct<MessageStructMember<V, K>...> : public Message {
private:
	using value_type = std::tuple<V...>;
	value_type values;
	friend class Builder;
	friend class Reader;

public:
	class Reader;
	class Builder {
	private:
		MessageStruct<MessageStructMember<V, K>...> &message;

	public:
		Builder(MessageStruct<MessageStructMember<V, K>...> &message)
			: message{message} {
			message.set_explicitly = true;
		}

		template <size_t i>
		constexpr typename std::tuple_element_t<i, value_type>::Builder init() {
			std::tuple_element_t<i, value_type> &msg_ref =
				std::get<i>(message.values);
			return
				typename std::tuple_element_t<i, value_type>::Builder{msg_ref};
		}

		template <typename T>
		constexpr
			typename std::tuple_element_t<ParameterPackIndex<T, K...>::value,
										  value_type>::Builder
			init() {
			std::tuple_element_t<ParameterPackIndex<T, K...>::value, value_type>
				&msg_ref = std::get<ParameterPackIndex<T, K...>::value>(
					message.values);
			return typename std::tuple_element_t<
				ParameterPackIndex<T, K...>::value, value_type>::Builder{
				msg_ref};
		}

		Reader asReader() { return Reader{message}; }
	};
	class Reader {
	private:
		MessageStruct<MessageStructMember<V, K>...> &message;

	public:
		Reader(MessageStruct<MessageStructMember<V, K>...> &message)
			: message{message} {}

		template <size_t i>
		constexpr typename std::tuple_element_t<i, value_type>::Reader get() {
			std::tuple_element_t<i, value_type> &msg_ref =
				std::get<i>(message.values);
			return
				typename std::tuple_element_t<i, value_type>::Reader{msg_ref};
		}

		template <typename T>
		constexpr
			typename std::tuple_element_t<ParameterPackIndex<T, K...>::value,
										  value_type>::Reader
			get() {
			std::tuple_element_t<ParameterPackIndex<T, K...>::value, value_type>
				&msg_ref = std::get<ParameterPackIndex<T, K...>::value>(
					message.values);
			return typename std::tuple_element_t<
				ParameterPackIndex<T, K...>::value, value_type>::Reader{
				msg_ref};
		}

		constexpr size_t size() { return std::tuple_size<value_type>::value; }

		Builder asBuilder() { return Builder{message}; }
	};
};

template <typename T, typename K> struct MessageUnionMember;

template <typename T, typename C, C... Chars>
struct MessageUnionMember<T, StringLiteral<C, Chars...>> {
	T value;
};

template <typename... T> class MessageUnion;

/// @todo copied from MessageStruct, but the acces is different, since
///	 only one value can be set at the same time.
template <typename... V, typename... K>
class MessageUnion<MessageUnionMember<V, K>...> : public Message {
private:
	using value_type = std::variant<MessageUnionMember<V,K>...>;
	value_type values;
	friend class Builder;
	friend class Reader;

public:
	class Reader;
	class Builder {
	private:
		MessageUnion<MessageUnionMember<V, K>...> &message;

	public:
		Builder(MessageUnion<MessageUnionMember<V, K>...> &message)
			: message{message} {
			message.set_explicitly = true;
		}

		template <size_t i>
		constexpr typename std::variant_alternative_t<i, value_type>::Builder
		init() {
			std::variant_alternative_t<i, value_type> &msg_ref =
				std::get<i>(message.values);
			return typename std::variant_alternative_t<i, value_type>::Builder{
				msg_ref};
		}

		template <typename T>
		constexpr typename std::variant_alternative_t<
			ParameterPackIndex<T, K...>::value, value_type>::Builder
		init() {
			std::variant_alternative_t<ParameterPackIndex<T, K...>::value,
									   value_type> &msg_ref =
				std::get<ParameterPackIndex<T, K...>::value>(message.values);
			return typename std::variant_alternative_t<
				ParameterPackIndex<T, K...>::value, value_type>::Builder{
				msg_ref};
		}

		Reader asReader() { return Reader{message}; }
	};
	class Reader {
	private:
		MessageUnion<MessageUnionMember<V, K>...> &message;

	public:
		Reader(MessageUnion<MessageUnionMember<V, K>...> &message)
			: message{message} {}

		template <size_t i>
		constexpr typename ParameterPackType<i, V...>::Reader
		get() {
			std::variant_alternative_t<i, value_type> &msg_ref =
				std::get<i>(message.values);
			return typename std::variant_alternative_t<i, value_type>::Reader{
				msg_ref};
		}

		template <typename T>
		constexpr typename ParameterPackType<<
			ParameterPackIndex<T, K...>::value, V...>::Reader
		get() {
			std::variant_alternative_t<ParameterPackIndex<T, K...>::value,
									   value_type> &msg_ref =
				std::get<ParameterPackIndex<T, K...>::value>(message.values);
			return typename std::variant_alternative_t<
				ParameterPackIndex<T, K...>::value, value_type>::Reader{
				msg_ref};
		}

		template <typename T> constexpr bool holdsAlternative() {
			return std::holds_alternative<std::variant_alternative_t<ParameterPackIndex<T,K...>, value_type>>(message.values);
		}

		size_t alternativeIndex() const { return }

		constexpr size_t size() { return std::variant_size<value_type>::value; }

		Builder asBuilder() { return Builder{message}; }
	};
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

inline MessageBuilder heapMessageBuilder() { return MessageBuilder{}; }
} // namespace gin
