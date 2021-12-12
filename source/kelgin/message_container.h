#pragma once

#include "schema.h"

namespace gin {
template <class T> class MessageContainer;

template <class T, class Container> class Message;

template <size_t N, class... T> struct MessageParameterPackType;

template <class TN, class... T> struct MessageParameterPackType<0, TN, T...> {
	using Type = T;
};

template <size_t N, class TN, class... T>
struct MessageParameterPackType<N, TN, T...> {
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

template <class... V, StringLiteral... Keys>
class MessageContainer<schema::Struct<schema::NamedMember<V, Keys>...>> {
private:
	using ValueType = std::tuple<Message<V, MessageContainer<V>>...>;
	ValueType values;

public:
	using SchemaType = schema::Struct<schema::NamedMember<V, Keys>...>;

	template <size_t i>
	using ElementType =
		MessageParameterPackType<i, Message<V, MessageContainer<V>>...>::Type;

	template <size_t i> ElementType<i> &get() { return std::get<i>(values); }
};

/*
 * Union storage
 */
template<class... V, StringLiteral... Keys>
class MessageContainer<schema::Union<schema::NamedMember<V,Keys>...>> {
private:
	using ValueType = std::variant<Message<V,MessageContainer<V>>...>;
	ValueType value;
public:
};

/*
 * Helper for the basic message container, so the class doesn't have to be
 * specialized 10 times.
 */
template <class T> struct PrimitiveTypeHelper;

struct PrimitiveTypeHelper<schema::Primitive<schema::SignedIntegral, 1>> {
	using Type = int8_t;
};

struct PrimitiveTypeHelper<schema::Primitive<schema::SignedIntegral, 2>> {
	using Type = int16_t;
};

struct PrimitiveTypeHelper<schema::Primitive<schema::SignedIntegral, 4>> {
	using Type = int32_t;
};

struct PrimitiveTypeHelper<schema::Primitive<schema::SignedIntegral, 8>> {
	using Type = int64_t;
};

struct PrimitiveTypeHelper<schema::Primitive<schema::UnsignedIntegral, 1>> {
	using Type = uint8_t;
};

struct PrimitiveTypeHelper<schema::Primitive<schema::UnsignedIntegral, 2>> {
	using Type = uint16_t;
};

struct PrimitiveTypeHelper<schema::Primitive<schema::UnsignedIntegral, 4>> {
	using Type = uint32_t;
};

struct PrimitiveTypeHelper<schema::Primitive<schema::UnsignedIntegral, 8>> {
	using Type = uint64_t;
};

struct PrimitiveTypeHelper<schema::Primitive<schema::FloatingPoint, 4>> {
	using Type = float;
};

struct PrimitiveTypeHelper<schema::Primitive<schema::FloatingPoint, 8>> {
	using Type = double;
};

template <class T, class N> class MessageContainer<schema::Primitive<T, N>> {
public:
	using ValueType = PrimitiveTypeHelper<schema::Primitive<T, N>>::Type;

private:
	ValueType value;

public:
	MessageContainer() : value{0} {}

	void set(const ValueType &v) { value = v; }

	const ValueType &get() const { return value; }
};

template <> class MessageContainer<schema::String> {
public:
private:
	using ValueType = std::string;
	ValueType value;

public:
	void set(const ValueType &v) { value = v; }

	const ValueType &get() const { return value; }
};
} // namespace gin
