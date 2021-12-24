#pragma once

#include "schema.h"

namespace gin {
template <class T> class MessageContainer;

template <class T, class Container> class Message;

template <size_t N, class... T> struct MessageParameterPackType;

template <class TN, class... T> struct MessageParameterPackType<0, TN, T...> {
	using Type = TN;
};

template <size_t N, class TN, class... T>
struct MessageParameterPackType<N, TN, T...> {
	static_assert(sizeof...(T) > 0, "Exhausted parameters");
	using Type = typename MessageParameterPackType<N - 1, T...>::Type;
};

template <class T, class... TL> struct MessageParameterPackIndex;

template <class T, class... TL> struct MessageParameterPackIndex<T, T, TL...> {
	static constexpr size_t Value = 0u;
};

template <class T, class TL0, class... TL>
struct MessageParameterPackIndex<T, TL0, TL...> {
	static constexpr size_t Value =
		1u + MessageParameterPackIndex<T, TL...>::Value;
};

/*
 * Nightmare inducing compiler problems found here. Somehow non-type
 * StringLiterals cannot be resolved as non-type primitive template values can.
 * This is the workaround
 */
template <StringLiteral V, StringLiteral Key0, StringLiteral... Keys>
struct MessageParameterKeyPackIndexHelper {
	static constexpr size_t Value =
		(V == Key0)
			? (0u)
			: (1u + MessageParameterKeyPackIndexHelper<V, Keys...>::Value);
};

template <StringLiteral V, StringLiteral Key0>
struct MessageParameterKeyPackIndexHelper<V, Key0> {
	static constexpr size_t Value = (V == Key0) ? (0u) : (1u);
};

template <StringLiteral V, StringLiteral... Keys>
struct MessageParameterKeyPackIndex {
	static constexpr size_t Value =
		MessageParameterKeyPackIndexHelper<V, Keys...>::Value;
	static_assert(Value < sizeof...(Keys),
				  "Provided StringLiteral doesn't exist in searched list");
};

template <class... V, StringLiteral... Keys>
class MessageContainer<schema::Struct<schema::NamedMember<V, Keys>...>> {
private:
	using ValueType = std::tuple<Message<V, MessageContainer<V>>...>;
	ValueType values;

public:
	using SchemaType = schema::Struct<schema::NamedMember<V, Keys>...>;

	template <size_t i>
	using ElementType = typename MessageParameterPackType<
		i, Message<V, MessageContainer<V>>...>::Type;

	template <size_t i> ElementType<i> &get() { return std::get<i>(values); }
};

/*
 * Union storage
 */
template <class... V, StringLiteral... Keys>
class MessageContainer<schema::Union<schema::NamedMember<V, Keys>...>> {
private:
	using ValueType = std::variant<Message<V, MessageContainer<V>>...>;
	ValueType value;

public:
	using SchemaType = schema::Union<schema::NamedMember<V, Keys>...>;

	template <size_t i>
	using ElementType = typename MessageParameterPackType<
		i, Message<V, MessageContainer<V>>...>::Type;

	template <size_t i> ElementType<i> &get() {
		if (i != value.index()) {
			using MessageIV = typename MessageParameterPackType<i, V...>::Type;
			value = Message<MessageIV, MessageContainer<MessageIV>>{};
		}
		return std::get<i>(value);
	}

	size_t index() const noexcept { return value.index(); }
};

/*
 * Array storage
 */

/*
template<class T>
class MessageContainer<schema::Array> {
private:
	using ValueType = std::vector<Message<T,MessageContainer<T>>>;
	ValueType values;
public:
	using SchemaType = schema::Array<T>;

	template<size_t i> Message<T,MessageContainer<T>>& get(){
		return values.at(i);
	}
};
*/

/*
 * Tuple storage
 */
template <class... T> class MessageContainer<schema::Tuple<T...>> {
private:
	using ValueType = std::tuple<Message<T, MessageContainer<T>>...>;
	ValueType values;

public:
	using SchemaType = schema::Tuple<T...>;

	template <size_t i>
	using ElementType = typename MessageParameterPackType<
		i, Message<T, MessageContainer<T>>...>::Type;

	template <size_t i> ElementType<i> &get() { return std::get<i>(values); }
};

/*
 * Helper for the basic message container, so the class doesn't have to be
 * specialized 10 times.
 */
template <class T> struct PrimitiveTypeHelper;

template <>
struct PrimitiveTypeHelper<schema::Primitive<schema::SignedInteger, 1>> {
	using Type = int8_t;
};

template <>
struct PrimitiveTypeHelper<schema::Primitive<schema::SignedInteger, 2>> {
	using Type = int16_t;
};

template <>
struct PrimitiveTypeHelper<schema::Primitive<schema::SignedInteger, 4>> {
	using Type = int32_t;
};

template <>
struct PrimitiveTypeHelper<schema::Primitive<schema::SignedInteger, 8>> {
	using Type = int64_t;
};

template <>
struct PrimitiveTypeHelper<schema::Primitive<schema::UnsignedInteger, 1>> {
	using Type = uint8_t;
};

template <>
struct PrimitiveTypeHelper<schema::Primitive<schema::UnsignedInteger, 2>> {
	using Type = uint16_t;
};

template <>
struct PrimitiveTypeHelper<schema::Primitive<schema::UnsignedInteger, 4>> {
	using Type = uint32_t;
};

template <>
struct PrimitiveTypeHelper<schema::Primitive<schema::UnsignedInteger, 8>> {
	using Type = uint64_t;
};

template <>
struct PrimitiveTypeHelper<schema::Primitive<schema::FloatingPoint, 4>> {
	using Type = float;
};

template <>
struct PrimitiveTypeHelper<schema::Primitive<schema::FloatingPoint, 8>> {
	using Type = double;
};

template <class T, size_t N> class MessageContainer<schema::Primitive<T, N>> {
public:
	using SchemaType = schema::Primitive<T, N>;
	using ValueType =
		typename PrimitiveTypeHelper<schema::Primitive<T, N>>::Type;

private:
	ValueType value;

public:
	MessageContainer() : value{0} {}

	void set(const ValueType &v) { value = v; }

	const ValueType &get() const { return value; }
};

template <> class MessageContainer<schema::String> {
public:
	using SchemaType = schema::String;
	using ValueType = std::string;

private:
	ValueType value;

public:
	void set(const ValueType &v) { value = v; }

	const ValueType &get() const { return value; }
};
} // namespace gin
