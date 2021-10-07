#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace gin {

#define GIN_CONCAT_(x, y) x##y
#define GIN_CONCAT(x, y) GIN_CONCAT_(x, y)
#define GIN_UNIQUE_NAME(prefix) GIN_CONCAT(prefix, __LINE__)

#define GIN_FORBID_COPY(classname)                                             \
	classname(const classname &) = delete;                                     \
	classname &operator=(const classname &) = delete

#define GIN_FORBID_MOVE(classname)                                             \
	classname(classname &&) = delete;                                          \
	classname &operator=(classname &&) = delete

#define GIN_DEFAULT_COPY(classname)                                            \
	classname(const classname &) = default;                                    \
	classname &operator=(const classname &) = default

#define GIN_DEFAULT_MOVE(classname)                                            \
	classname(classname &&) = default;                                         \
	classname &operator=(classname &&) = default

#define GIN_ASSERT(expression)                                                 \
	assert(expression);                                                        \
	if (!(expression))

template <typename T> using Maybe = std::optional<T>;

template <typename T> using Own = std::unique_ptr<T>;

template <typename T> using Our = std::shared_ptr<T>;

template <typename T> using Lent = std::weak_ptr<T>;

template <typename T, class... Args> Own<T> heap(Args &&...args) {
	return Own<T>(new T(std::forward<Args>(args)...));
}

template <typename T, class... Args> Our<T> share(Args &&...args) {
	return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T> T instance() noexcept;

template <typename Func, typename T> struct ReturnTypeHelper {
	typedef decltype(instance<Func>()(instance<T>())) Type;
};
template <typename Func> struct ReturnTypeHelper<Func, void> {
	typedef decltype(instance<Func>()()) Type;
};

template <typename Func, typename T>
using ReturnType = typename ReturnTypeHelper<Func, T>::Type;

struct Void {};

template <typename T> struct VoidFix { typedef T Type; };
template <> struct VoidFix<void> { typedef Void Type; };
template <typename T> using FixVoid = typename VoidFix<T>::Type;

template <typename T> struct VoidUnfix { typedef T Type; };
template <> struct VoidUnfix<Void> { typedef void Type; };
template <typename T> using UnfixVoid = typename VoidUnfix<T>::Type;

} // namespace gin
