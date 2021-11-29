#pragma once

#include <array>
#include <string_view>

namespace gin {
/**
 * Helper object which creates a templated string from the provided string
 * literal. It guarantees compile time uniqueness and thus allows using strings
 * in template parameters.
 */
template <template CharT, size_t N> class StringLiteral {
public:
	constexpr StringLiteral(const CharT (&input)[N]) noexcept {
		for(size_t i = 0; i < N ; ++i){
			data[i] = input[i];
		}
	}

	std::array<CharT, N> data{};

	constexpr std::string_view view() const noexcept {
		return std::string_view{data.data()};
	}
};

template <typename T, T... Chars>
constexpr gin::StringLiteral<T, sizeof...(Chars)> operator""_key() {
	return gin::StringLiteral<T,sizeof...(Chars)+1u>{Chars..., '\0'};
}
} // namespace gin
