#include "suite/suite.h"

#include <cstdint>
#include <string>

#include "source/message.h"
using gin::MessageList;
using gin::MessageStruct;
using gin::MessageStructMember;
using gin::MessagePrimitive;
using gin::heapMessageBuilder;

namespace {
typedef MessageList<MessagePrimitive<uint32_t>, MessagePrimitive<std::string> > TestList;

GIN_TEST("MessageList"){
	std::string test_string_1 = "banana";
	
	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestList>();
	auto uint = root.init<0>();
	uint.set(10);
	auto string = root.init<1>();
	string.set(test_string_1);

	auto root_reader = root.asReader();
	auto uint_reader = root_reader.get<0>();
	auto string_reader = root_reader.get<1>();
	
	GIN_EXPECT( uint_reader.get() == 10 && string_reader.get() == test_string_1, "wrong values");
}

typedef MessageList<MessageList<MessagePrimitive<uint32_t>, MessagePrimitive<std::string>>, MessagePrimitive<std::string> > NestedTestList;

GIN_TEST("MessageList nested"){
	std::string test_string_1 = "banana";
	std::string test_string_2 = "bat";
	
	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<NestedTestList>();
	auto inner_list = root.init<0>();
	auto uint = inner_list.init<0>();
	uint.set(20);
	auto inner_string = inner_list.init<1>();
	inner_string.set(test_string_2);
	
	auto string = root.init<1>();
	string.set(test_string_1);

	auto root_reader = root.asReader();
	auto inner_reader = root_reader.get<0>();
	auto uint_reader = inner_reader.get<0>();
	auto inner_string_reader = inner_reader.get<1>();
	auto string_reader = root_reader.get<1>();
	
	GIN_EXPECT(uint_reader.get() == 20 && inner_string_reader.get() == test_string_2 && string_reader.get() == test_string_1, "wrong values");
}

typedef MessageStruct<
	MessageStructMember<MessagePrimitive<uint32_t>, decltype("test_uint"_t)>,
	MessageStructMember<MessagePrimitive<std::string>, decltype("test_string"_t)>,
	MessageStructMember<MessagePrimitive<std::string>, decltype("test_name"_t)>
> TestStruct;

GIN_TEST("MessageStruct"){
	std::string test_string = "foo";
	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestStruct>();
	auto uint = root.init<decltype("test_uint"_t)>();
	uint.set(23);
	auto string = root.init<decltype("test_string"_t)>();
	string.set(test_string);
	auto string_name = root.init<decltype("test_name"_t)>();
	string_name.set(&"test_name"_t.data[0]);

	auto reader = root.asReader();
	auto uint_reader = reader.get<decltype("test_uint"_t)>();
	auto string_reader = reader.get<decltype("test_string"_t)>();
	auto name_reader = reader.get<decltype("test_name"_t)>();

	test_string = "foo2";

	GIN_EXPECT(uint_reader.get() == 23 && string_reader.get() != test_string && string_reader.get() == "foo" && name_reader.get() == "test_name", "wrong values");
}
}