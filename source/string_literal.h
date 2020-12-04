#pragma once

#include <array>
#include <string_view>

namespace gin {
/**
 * Helper object which creates a templated string from the provided string
 * literal. It guarantees compile time uniqueness and thus allows using strings
 * in template parameters.
 */
template <typename T, T... Chars> class StringLiteral {
public:
	static constexpr std::array<T, sizeof...(Chars) + 1u> data = {Chars...,
																  '\0'};
	static constexpr std::string_view view() {
		return std::string_view{data.data()};
	}
};
} // namespace gin

template <typename T, T... Chars>
constexpr gin::StringLiteral<T, Chars...> operator""_t() {
	return {};
}