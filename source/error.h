#pragma once

#include <string>
#include <variant>

#include "common.h"

namespace gin {
/**
 * Utility class for generating errors. Has a base distinction between
 * critical and recoverable errors. Additional code ids can be provided to the
 * constructor if additional distinctions are necessary.
 */
class Error {
private:
	std::string error_message;
	int8_t error_;

public:
	Error();
	Error(const std::string &msg);
	Error(const std::string &msg, int8_t code);
	Error(const Error &error);
	Error(Error &&error);

	Error &operator=(const Error &) = default;
	Error &operator=(Error &&) = default;

	const std::string &message() const;
	bool failed() const;

	bool isCritical() const;
	bool isRecoverable() const;
};

Error criticalError(const std::string &msg);
Error recoverableError(const std::string &msg);
Error noError();

/**
 * Exception alternative. Since I code without exceptions this class is
 * essentially a kind of exception replacement.
 */
template <typename T> class ErrorOr;

class ErrorOrValue {
public:
	virtual ~ErrorOrValue() = default;

	template <typename T> ErrorOr<T> &as() {
		return reinterpret_cast<ErrorOr<T> &>(*this);
	}

	template <typename T> const ErrorOr<T> &as() const {
		return reinterpret_cast<const ErrorOr<T> &>(*this);
	}
};

template <typename T> class ErrorOr : public ErrorOrValue {
private:
	std::variant<T, Error> value_or_error;

public:
	ErrorOr() = default;
	ErrorOr(const T &value) : value_or_error{value} {}

	ErrorOr(T &&value) : value_or_error{std::move(value)} {}

	ErrorOr(const Error &error) : value_or_error{error} {}
	ErrorOr(Error &&error) : value_or_error{std::move(error)} {}

	bool isValue() const { return std::holds_alternative<T>(value_or_error); }

	bool isError() const {
		return std::holds_alternative<Error>(value_or_error);
	}

	Error &error() { return std::get<Error>(value_or_error); }

	const Error &error() const { return std::get<Error>(value_or_error); }

	T &value() { return std::get<T>(value_or_error); }

	const T &value() const { return std::get<T>(value_or_error); }
};

} // namespace gin