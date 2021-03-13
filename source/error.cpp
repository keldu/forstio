#include "error.h"

namespace gin {
Error::Error() : error_{0} {}

Error::Error(const std::string_view &msg, int8_t code)
	: error_message{msg}, error_{code} {}

Error::Error(std::string &&msg, int8_t code)
	: error_message{std::move(msg)}, error_{code} {}

Error::Error(const Error &error)
	: error_message{error.error_message}, error_{error.error_} {}

Error::Error(Error &&error)
	: error_message{std::move(error.error_message)}, error_{std::move(
														 error.error_)} {}

const std::string_view Error::message() const { return error_message; }

bool Error::failed() const { return error_ != 0; }

bool Error::isCritical() const { return error_ < 0; }

bool Error::isRecoverable() const { return error_ > 0; }

Error Error::copyError() const {
	Error error;
	error.error_ = error_;
	try {
		error.error_message = error_message;
	} catch (std::bad_alloc &) {
		error.error_message = std::string_view{};
	}
	return error;
}

Error noError() { return Error{}; }

} // namespace gin
