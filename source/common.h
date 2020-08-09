#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace gin {

#define GIN_CONCAT_(x, y) x##y
#define GIN_CONCAT(x, y) GIN_CONCAT_(x, y)
#define GIN_UNIQUE_NAME(prefix) GIN_CONCAT(prefix, __LINE__)

#define GIN_FORBID_COPY(classname)      \
  classname(const classname&) = delete; \
  classname& operator=(const classname&) = delete

template <typename T>
using Maybe = std::optional<T>;

template <typename T>
using Own = std::unique_ptr<T>;

template <typename T>
using Our = std::shared_ptr<T>;

template <typename T>
using Lent = std::weak_ptr<T>;

template <typename T, class... Args>
Own<T> heap(Args&&... args) {
  return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T, class... Args>
Our<T> share(Args&&... args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

struct Void {};

template <typename T>
T instance() noexcept;

}  // namespace gin