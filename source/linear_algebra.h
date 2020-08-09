#pragma once

#include <array>
#include <cstdint>

namespace gin {
template <typename T, size_t M, size_t N> class Matrix {
  private:
	std::array<T, M * N> data;

  public:
	Matrix();

	T &operator()(size_t i, size_t j);
	const T &operator()(size_t i, size_t j) const;

	template <size_t K>
	Matrix<T, M, K> operator*(const Matrix<T, N, K> &rhs) const;

	Vector<T, M> operator*(const Vector<T, N> &rhs) const;
};

template <typename T, size_t N> class Vector {
  private:
	std::array<T, N> data;

  public:
	Vector();

	T operator*(const Vector<T, N> &rhs) const;

	T &operator()(size_t i);
	const T &operator()(size_t i) const;
};
} // namespace gin

namespace gin {
// column major is "i + j * M";
template <typename T, size_t M, size_t N>
T &Matrix<T, M, N>::operator()(size_t i, size_t j) {
	assert(i < M && j < N);
	return data[i * N + j];
}
template <typename T, size_t M, size_t N>
const T &Matrix<T, M, N>::operator()(size_t i, size_t j) const {
	assert(i < M && j < N);
	return data[i * N + j];
}
} // namespace gin