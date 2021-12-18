#include "suite/suite.h"

#include <cstdint>
#include <string>

#include "source/kelgin/message.h"
using gin::heapMessageBuilder;

namespace {
using TestTuple = schema::Tuple<schema::UInt32, schema::String>;

GIN_TEST("MessageList"){
	std::string test_string_1 = "banana";
	
	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestTuple>();
	auto uint = root.init<0>();
	uint.set(10);
	auto string = root.init<1>();
	string.set(test_string_1);

	auto root_reader = root.asReader();
	auto uint_reader = root_reader.get<0>();
	auto string_reader = root_reader.get<1>();
	
	GIN_EXPECT( uint_reader.get() == 10 && string_reader.get() == test_string_1, "wrong values");
}

using NestedTestTuple = schema::Tuple<schema::Tuple<schema::UInt32, schema::String>, schema::String>;

GIN_TEST("MessageList nested"){
	std::string test_string_1 = "banana";
	std::string test_string_2 = "bat";
	
	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<NestedTestTuple>();
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

using TestStruct = schema::Struct<
	schema::NamedMember<schema::UInt32, "test_uint">,
	schema::NamedMember<schema::String, "test_string">,
	schema::NamedMember<schema::String, "test_name">
>;

GIN_TEST("MessageStruct"){
	std::string test_string = "foo";
	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestStruct>();
	auto uint = root.init<"test_uint">();
	uint.set(23);
	auto string = root.init<"test_string">();
	string.set(test_string);
	auto string_name = root.init<"test_name">();
	string_name.set(&"test_name"_t.data[0]);

	auto reader = root.asReader();
	auto uint_reader = reader.get<"test_uint">();
	auto string_reader = reader.get<"test_string">();
	auto name_reader = reader.get<"test_name">();

	/*
	 * Set string to another value to guarantee no changes
	 */
	test_string = "foo2";

	GIN_EXPECT(uint_reader.get() == 23 && string_reader.get() != test_string && string_reader.get() == "foo" && name_reader.get() == "test_name", "wrong values");
}
}
