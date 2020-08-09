#pragma once

#include <string>
#include <variant>

namespace gin {
class Error {
private:
	std::string error_message;
	int8_t error_;

public:
	Error();
	Error(const std::string &msg);
	Error(const std::string &msg, int8_t code);
	Error(const Error &error);

	const std::string &message() const;
	bool failed() const;

	bool isCritical() const;
	bool isRecoverable() const;
};

Error criticalError(const std::string &msg);
Error recoverableError(const std::string &msg);
Error noError();

class ErrorOrValue {
public:
	virtual ~ErrorOrValue() = default;
};

template <typename T> class ErrorOr : public ErrorOrValue {
private:
	std::variant<T, Error> value_or_error;

public:
	ErrorOr(const T &value) : value_or_error{value} {}

	ErrorOr(T &&value) : value_or_error{std::move(value)} {}

	ErrorOr(const Error &error) : value_or_error{error} {}
	ErrorOr(Error &&error) : value_or_error{std::move(error)} {}

	bool isValue() const { return std::holds_alternative<T>(value_or_error); }

	bool isError() const {
		return std::holds_alternative<Error>(value_or_error);
	}

	const Error &error() const { return std::get<Error>(value_or_error); }

	T &value() { return std::get<T>(value_or_error); }

	const T &value() const { return std::get<T>(value_or_error); }
};
} // namespace gin
