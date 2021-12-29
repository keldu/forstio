#include "error.h"

namespace saw {
Error::Error() : error_{static_cast<Error::Code>(0)} {}

Error::Error(const std::string_view &msg, Error::Code code)
	: error_message{msg}, error_{static_cast<Error::Code>(code)} {}

Error::Error(std::string &&msg, Error::Code code)
	: error_message{std::move(msg)}, error_{static_cast<Error::Code>(code)} {}

Error::Error(Error &&error)
	: error_message{std::move(error.error_message)}, error_{std::move(
														 error.error_)} {}

const std::string_view Error::message() const {

	return std::visit(
		[this](auto &&arg) -> const std::string_view {
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, std::string>) {
				return std::string_view{arg};
			} else if constexpr (std::is_same_v<T, std::string_view>) {
				return arg;
			} else {
				return "Error in class Error. Good luck :)";
			}
		},
		error_message);
}

bool Error::failed() const {
	return static_cast<std::underlying_type_t<Error::Code>>(error_) != 0;
}

bool Error::isCritical() const {
	return static_cast<std::underlying_type_t<Error::Code>>(error_) < 0;
}

bool Error::isRecoverable() const {
	return static_cast<std::underlying_type_t<Error::Code>>(error_) > 0;
}

Error Error::copyError() const {
	Error error;
	error.error_ = error_;
	try {
		error.error_message = error_message;
	} catch (const std::bad_alloc &) {
		error.error_message =
			std::string_view{"Error while copying Error string. Out of memory"};
	}
	return error;
}

Error::Code Error::code() const { return static_cast<Error::Code>(error_); }

Error makeError(const std::string_view &generic, Error::Code code) {
	return Error{generic, code};
}

Error criticalError(const std::string_view &generic, Error::Code c) {
	return makeError(generic, c);
}

Error recoverableError(const std::string_view &generic, Error::Code c) {
	return makeError(generic, c);
}

Error noError() { return Error{}; }

} // namespace saw
