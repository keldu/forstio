#pragma once

#include <string>
#include <string_view>
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
	std::variant<std::string_view, std::string> error_message;
	int8_t error_;

public:
	Error();
	Error(const std::string_view &msg, int8_t code);
	Error(std::string &&msg, int8_t code);
	Error(Error &&error);

	GIN_FORBID_COPY(Error);

	Error &operator=(Error &&) = default;

	const std::string_view message() const;
	bool failed() const;

	bool isCritical() const;
	bool isRecoverable() const;

	Error copyError() const;
};

template <typename Formatter>
Error makeError(const Formatter &formatter, int8_t code,
				const std::string_view &generic) {
	try {
		std::string error_msg = formatter();
		return Error{std::move(error_msg), code};
	} catch (std::bad_alloc &) {
		return Error{generic, code};
	}
}

Error criticalError(const std::string_view &generic);

template <typename Formatter>
Error criticalError(const Formatter &formatter,
					const std::string_view &generic) {
	return makeError(formatter, -1, generic);
}

Error recoverableError(const std::string_view &generic);

template <typename Formatter>
Error recoverableError(const Formatter &formatter,
					   const std::string_view &generic) {
	return makeError(formatter, -1, generic);
}

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
		return dynamic_cast<ErrorOr<T> &>(*this);
	}

	template <typename T> const ErrorOr<T> &as() const {
		return dynamic_cast<const ErrorOr<T> &>(*this);
	}
};

template <typename T> class ErrorOr : public ErrorOrValue {
private:
	std::variant<FixVoid<T>, Error> value_or_error;

public:
	ErrorOr() = default;
	ErrorOr(const FixVoid<T> &value) : value_or_error{value} {}

	ErrorOr(FixVoid<T> &&value) : value_or_error{std::move(value)} {}

	ErrorOr(const Error &error) : value_or_error{error} {}
	ErrorOr(Error &&error) : value_or_error{std::move(error)} {}

	bool isValue() const { return std::holds_alternative<T>(value_or_error); }

	bool isError() const {
		return std::holds_alternative<Error>(value_or_error);
	}

	Error &error() { return std::get<Error>(value_or_error); }

	const Error &error() const { return std::get<Error>(value_or_error); }

	FixVoid<T> &value() { return std::get<FixVoid<T>>(value_or_error); }

	const FixVoid<T> &value() const {
		return std::get<FixVoid<T>>(value_or_error);
	}
};

} // namespace gin
