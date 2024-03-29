#pragma once

#include <string>
#include <string_view>
#include <variant>

#include "common.h"

namespace saw {
/**
 * Utility class for generating errors. Has a base distinction between
 * critical and recoverable errors. Additional code ids can be provided to the
 * constructor if additional distinctions are necessary.
 */
class Error {
public:
	enum class Code : int16_t {
		GenericCritical = -1,
		GenericRecoverable = 1,
		Disconnected = -99,
		Exhausted = -98
	};

private:
	std::variant<std::string_view, std::string> error_message;
	Code error_;

public:
	Error();
	Error(const std::string_view &msg, Error::Code code);
	Error(std::string &&msg, Error::Code code);
	Error(Error &&error);

	SAW_FORBID_COPY(Error);

	Error &operator=(Error &&) = default;

	const std::string_view message() const;
	bool failed() const;

	bool isCritical() const;
	bool isRecoverable() const;

	Error copyError() const;

	Code code() const;
};

Error makeError(const std::string_view &generic, Error::Code c);

template <typename Formatter>
Error makeError(const Formatter &formatter, Error::Code code,
				const std::string_view &generic) {
	try {
		std::string error_msg = formatter();
		return Error{std::move(error_msg), code};
	} catch (std::bad_alloc &) {
		return Error{generic, code};
	}
}

Error criticalError(const std::string_view &generic,
					Error::Code c = Error::Code::GenericCritical);

template <typename Formatter>
Error criticalError(const Formatter &formatter, const std::string_view &generic,
					Error::Code c = Error::Code::GenericCritical) {
	return makeError(formatter, c, generic);
}

Error recoverableError(const std::string_view &generic,
					   Error::Code c = Error::Code::GenericRecoverable);

template <typename Formatter>
Error recoverableError(const Formatter &formatter,
					   const std::string_view &generic,
					   Error::Code c = Error::Code::GenericRecoverable) {
	return makeError(formatter, c, generic);
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

	template <typename T> ErrorOr<UnfixVoid<T>> &as() {
		return static_cast<ErrorOr<UnfixVoid<T>> &>(*this);
	}

	template <typename T> const ErrorOr<UnfixVoid<T>> &as() const {
		return static_cast<const ErrorOr<UnfixVoid<T>> &>(*this);
	}
};

template <typename T> class ErrorOr final : public ErrorOrValue {
private:
	std::variant<Error, FixVoid<T>> value_or_error;

	static_assert(!std::is_same_v<T, Void>, "Don't use internal private types");

public:
	ErrorOr() = default;
	ErrorOr(const FixVoid<T> &value) : value_or_error{value} {}

	ErrorOr(FixVoid<T> &&value) : value_or_error{std::move(value)} {}

	ErrorOr(const Error &error) : value_or_error{error} {}
	ErrorOr(Error &&error) : value_or_error{std::move(error)} {}

	bool isValue() const {
		return std::holds_alternative<FixVoid<T>>(value_or_error);
	}

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

template <typename T> class ErrorOr<ErrorOr<T>> {
private:
	ErrorOr() = delete;
};

} // namespace saw
