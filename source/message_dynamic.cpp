#include "message_dynamic.h"

namespace gin {
template <>
DynamicMessage::Type DynamicMessagePrimitive<uint64_t>::type() const {
	return DynamicMessage::Type::Unsigned;
};
template <>
DynamicMessage::Type DynamicMessagePrimitive<int64_t>::type() const {
	return DynamicMessage::Type::Signed;
};
template <>
DynamicMessage::Type DynamicMessagePrimitive<std::string>::type() const {
	return DynamicMessage::Type::String;
};
template <> DynamicMessage::Type DynamicMessagePrimitive<bool>::type() const {
	return DynamicMessage::Type::Bool;
};
template <> DynamicMessage::Type DynamicMessagePrimitive<double>::type() const {
	return DynamicMessage::Type::Double;
};
} // namespace gin