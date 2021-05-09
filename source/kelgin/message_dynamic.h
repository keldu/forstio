#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <string>

#include "common.h"

/// @todo Move implementation to cpp file

namespace gin {
/*
 * Base class for each message data structure
 */
class DynamicMessage {
protected:
	/*
	 * The encoder and decoders use this as a hint if this was set by a default
	 * value or not
	 */
	bool set_explicitly = false;

public:
	/*
	 * Later use for dynamic access
	 */
	enum class Type : uint16_t {
		Null,
		Struct,
		List,
		Array,
		Union,
		String,
		Signed,
		Unsigned,
		Bool,
		Double
	};

	virtual ~DynamicMessage() = default;

	virtual Type type() const = 0;

	template <typename T> T &as() {
		static_assert(std::is_base_of<DynamicMessage, T>());
		return reinterpret_cast<T &>(*this);
	}

	template <typename T> const T &as() const {
		static_assert(std::is_base_of<DynamicMessage, T>());
		return reinterpret_cast<const T &>(*this);
	}

	class DynamicReader;
	class DynamicBuilder {
	private:
		DynamicMessage &message;

	public:
		DynamicBuilder(DynamicMessage &message) : message{message} {}

		DynamicReader asReader() const { return DynamicReader{message}; }

		DynamicMessage::Type type() const { return message.type(); }

		template <typename T> typename T::Builder as() {
			static_assert(std::is_base_of<DynamicMessage, T>());
			return typename T::Builder{reinterpret_cast<T &>(message)};
		}
	};

	class DynamicReader {
	private:
		DynamicMessage &message;

	public:
		DynamicReader(DynamicMessage &message) : message{message} {}

		DynamicBuilder asBuilder() const { return DynamicBuilder{message}; }

		DynamicMessage::Type type() const { return message.type(); }

		template <typename T> typename T::Reader as() {
			static_assert(std::is_base_of<DynamicMessage, T>());
			return typename T::Reader{reinterpret_cast<T &>(message)};
		}
	};
};

class DynamicMessageNull : public DynamicMessage {
public:
	DynamicMessage::Type type() const override {
		return DynamicMessage::Type::Null;
	}
};
static DynamicMessageNull dynamicNullMessage;

template <typename T> class DynamicMessagePrimitive : public DynamicMessage {
private:
	T value;
	friend class Builder;
	friend class Reader;

public:
	DynamicMessagePrimitive() = default;

	DynamicMessage::Type type() const override;

	class Reader;
	class Builder {
	private:
		DynamicMessagePrimitive<T> &message;

	public:
		Builder(DynamicMessagePrimitive<T> &message) : message{message} {}

		constexpr void set(const T &value) {
			message.value = value;
			message.set_explicitly = true;
		}

		constexpr void set(T &&value) {
			message.value = std::move(value);
			message.set_explicitly = true;
		}

		Reader asReader() const { return Reader{message}; }
	};

	class Reader {
	private:
		DynamicMessagePrimitive<T> &message;

	public:
		Reader(DynamicMessagePrimitive<T> &message) : message{message} {}

		constexpr const T &get() { return message.value; }

		bool isSetExplicitly() const { return message.set_explicitly; }

		Builder asBuilder() const { return Builder{message}; }
	};
};

using DynamicMessageString = DynamicMessagePrimitive<std::string>;
using DynamicMessageSigned = DynamicMessagePrimitive<int64_t>;
using DynamicMessageUnsigned = DynamicMessagePrimitive<uint64_t>;
using DynamicMessageBool = DynamicMessagePrimitive<bool>;
using DynamicMessageDouble = DynamicMessagePrimitive<double>;

class DynamicMessageStruct : public DynamicMessage {
private:
	std::map<std::string, Own<DynamicMessage>, std::less<>> messages;
	friend class Builder;
	friend class Reader;

public:
	DynamicMessage::Type type() const override {
		return DynamicMessage::Type::Struct;
	}
	class Reader;
	class Builder {
	private:
		DynamicMessageStruct &message;

	public:
		Builder(DynamicMessageStruct &message) : message{message} {
			message.set_explicitly = true;
		}

		template <typename T> typename T::Builder init(const std::string &key) {
			Own<T> msg = std::make_unique<T>();
			typename T::Builder builder{*msg};
			/*auto insert = */ message.messages.insert(
				std::make_pair(key, std::move(msg)));
			return builder;
		}

		DynamicMessage::DynamicBuilder init(const std::string &key,
											Own<DynamicMessage> &&msg) {
			DynamicMessage::DynamicBuilder builder{*msg};
			message.messages.insert(std::make_pair(key, std::move(msg)));
			return builder;
		}

		Reader asReader() const { return Reader{message}; }
	};

	class Reader {
	private:
		DynamicMessageStruct &message;

	public:
		Reader(DynamicMessageStruct &message) : message{message} {}

		DynamicMessage::DynamicReader get(std::string_view key) {
			auto find = message.messages.find(key);
			if (find != message.messages.end()) {
				return DynamicMessage::DynamicReader{*(find->second)};
			} else {
				return DynamicMessage::DynamicReader{dynamicNullMessage};
			}
		}

		size_t size() const { return message.messages.size(); }

		bool isSetExplicitly() const { return message.set_explicitly; }

		Builder asBuilder() const { return Builder{message}; }
	};
};

class DynamicMessageList : public DynamicMessage {
private:
	std::deque<Own<DynamicMessage>> messages;
	friend class Builder;
	friend class Reader;

public:
	DynamicMessage::Type type() const override {
		return DynamicMessage::Type::List;
	}
	class Reader;
	class Builder {
	private:
		DynamicMessageList &message;

	public:
		Builder(DynamicMessageList &message) : message{message} {
			message.set_explicitly = true;
		}

		template <typename T> typename T::Builder push() {
			static_assert(std::is_base_of<DynamicMessage, T>());
			Own<T> msg = std::make_unique<T>();
			typename T::Builder builder{*msg};
			message.messages.push_back(std::move(msg));
			return builder;
		}

		DynamicMessage::DynamicBuilder push(Own<DynamicMessage> &&msg) {
			DynamicMessage::DynamicBuilder builder{*msg};
			message.messages.push_back(std::move(msg));
			return builder;
		}

		Reader asReader() const { return Reader{message}; }
	};

	class Reader {
	private:
		DynamicMessageList &message;

	public:
		Reader(DynamicMessageList &message) : message{message} {}

		DynamicMessage::DynamicReader get(size_t element) {
			if (element < message.messages.size()) {
				return DynamicMessage::DynamicReader{
					*(message.messages[element])};
			} else {
				return DynamicMessage::DynamicReader{dynamicNullMessage};
			}
		}

		size_t size() const { return message.messages.size(); }

		bool isSetExplicitly() const { return message.set_explicitly; }

		Builder asBuilder() const { return Builder{message}; }
	};
};
} // namespace gin