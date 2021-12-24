#pragma once

#include "string_literal.h"

namespace gin {
namespace schema {

template <class T, StringLiteral Literal> struct NamedMember {};

template <class... T> struct Struct;

template <class... V, StringLiteral... K>
struct Struct<NamedMember<V, K>...> {};

template <class... T> struct Union;

template <class... V, StringLiteral... K> struct Union<NamedMember<V, K>...> {};

template <class T> struct Array {};

template <class... T> struct Tuple {};

struct String {};

struct SignedInteger {};
struct UnsignedInteger {};
struct FloatingPoint {};

template <class T, size_t N> struct Primitive {
	static_assert(((std::is_same_v<T, SignedInteger> ||
					std::is_same_v<T, UnsignedInteger>)&&(N == 1 || N == 2 ||
														  N == 4 || N == 8)) ||
					  (std::is_same_v<T, FloatingPoint> && (N == 4 || N == 8)),
				  "Primitive Type is not supported");
};

using Int8 = Primitive<SignedInteger, 1>;
using Int16 = Primitive<SignedInteger, 2>;
using Int32 = Primitive<SignedInteger, 4>;
using Int64 = Primitive<SignedInteger, 8>;

using UInt8 = Primitive<UnsignedInteger, 1>;
using UInt16 = Primitive<UnsignedInteger, 2>;
using UInt32 = Primitive<UnsignedInteger, 4>;
using UInt64 = Primitive<UnsignedInteger, 8>;

using Float32 = Primitive<FloatingPoint, 4>;
using Float64 = Primitive<FloatingPoint, 8>;

} // namespace schema
} // namespace gin
