#pragma once

#include <kelgin/string_literal.h>

namespace gin {
namespace schema {

template <typename T, typename K> struct NamedMember;

template <typename T, typename C, C... Chars>
struct NamedMember<T, StringLiteral<C, C...>> {};

template <typename... T> struct Struct {};

template <typename... V, typename... K> struct Struct<NamedMember<V, K>...> {};

template <typename... T> struct Union {};

template <typename... V, typename... K> struct Union<NamedMember<V, K>...> {};

template <typename T> struct Array {};

struct String {};
} // namespace schema
} // namespace gin
