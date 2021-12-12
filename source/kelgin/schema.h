#pragma once

#include "string_literal.h"

namespace gin {
namespace schema {

template <class T, StringLiteral Literal> struct NamedMember {};

template <class... T> struct Struct;

template <class... V, class... K> struct Struct<NamedMember<V, K>...> {};

template <class... T> struct Union;

template <class... V, class... K> struct Union<NamedMember<V, K>...> {};

template <class T> struct Array {};

template <class... T> struct Tuple {};

struct String {};

struct SignedIntegral {};
struct UnsignedIntegral {};
struct FloatingPoint {};

template <class T, size_t N> struct Primitive {
	static_assert(((std::is_same_v<T, SignedIntegral> ||
					std::is_same_v<T, UnsignedIntegral>)&&(N == 1 || N == 2 ||
														   N == 4 || N == 8)) ||
					  (std::is_same_v<T, FloatingPoint> && (N == 4 || N == 8)),
				  "Primitive Type is not supported");
};

using Int8 = Primitive<SignedIntegral, 1>;
using Int16 = Primitive<SignedIntegral, 2>;
using Int32 = Primitive<SignedIntegral, 4>;
using Int64 = Primitive<SignedIntegral, 8>;

using UInt8 = Primitive<UnsignedIntegral, 1>;
using UInt16 = Primitive<UnsignedIntegral, 2>;
using UInt32 = Primitive<UnsignedIntegral, 4>;
using UInt64 = Primitive<UnsignedIntegral, 8>;

using Float32 = Primitive<FloatingPoint, 4>;
using Float64 = Primitive<FloatingPoint, 8>;

} // namespace schema
} // namespace gin
