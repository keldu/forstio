#include "error.h"

namespace gin {
Error::Error() : error_{0} {}

Error::Error(const std::string &msg) : error_message{msg}, error_{1} {}

Error::Error(const std::string &msg, int8_t code)
	: error_message{msg}, error_{code} {}

Error::Error(const Error &error)
	: error_message{error.error_message}, error_{error.error_} {}

Error::Error(Error &&error)
	: error_message{std::move(error.error_message)}, error_{std::move(
														 error.error_)} {}

const std::string &Error::message() const { return error_message; }

bool Error::failed() const { return error_ != 0; }

bool Error::isCritical() const { return error_ < 0; }

bool Error::isRecoverable() const { return error_ > 0; }

Error criticalError(const std::string &msg) { return Error{msg, -1}; }

Error recoverableError(const std::string &msg) { return Error{msg, 1}; }

Error noError() { return Error{}; }
} // namespace gin
