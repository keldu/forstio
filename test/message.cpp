#include "suite/suite.h"

#include <cstdint>
#include <string>

#include "source/kelgin/message.h"
#include "source/kelgin/schema.h"

namespace {
namespace schema {
	using namespace gin::schema;
}

using TestTuple = schema::Tuple<schema::UInt32, schema::String>;

GIN_TEST("Message Tuple"){
	std::string test_string_1 = "banana";
	
	auto root = gin::heapMessageRoot<TestTuple>();
	auto builder = root.build();
	auto uint = builder.init<0>();
	uint.set(10);
	auto string = builder.init<1>();
	string.set(std::string_view{test_string_1});

	auto reader = root.read();
	auto uint_reader = reader.get<0>();
	auto string_reader = reader.get<1>();
	
	GIN_EXPECT( uint_reader.get() == 10 && string_reader.get() == test_string_1, "wrong values");
}

using NestedTestTuple = schema::Tuple<schema::Tuple<schema::UInt32, schema::String>, schema::String>;

GIN_TEST("Message Tuple nested"){
	std::string test_string_1 = "banana";
	std::string test_string_2 = "bat";
	
	auto root = gin::heapMessageRoot<NestedTestTuple>();
	auto builder = root.build();
	auto inner_list = builder.init<0>();
	auto uint = inner_list.init<0>();
	uint.set(20);
	auto inner_string = inner_list.init<1>();
	inner_string.set(test_string_2);
	
	auto string = builder.init<1>();
	string.set(test_string_1);

	auto root_reader = root.read();
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

GIN_TEST("Message Struct"){
	std::string test_string = "foo";
	auto root = gin::heapMessageRoot<TestStruct>();
	auto builder = root.build();
	auto uint = builder.init<"test_uint">();
	uint.set(23);
	auto string = builder.init<"test_string">();
	string.set(test_string);
	auto string_name = builder.init<"test_name">();
	string_name.set("test_name");

	auto reader = root.read();
	auto uint_reader = reader.get<"test_uint">();
	auto string_reader = reader.get<"test_string">();
	auto name_reader = reader.get<"test_name">();

	/*
	 * Set string to another value to guarantee no changes
	 */
	test_string = "foo2";

	GIN_EXPECT(uint_reader.get() == 23 && string_reader.get() != test_string && string_reader.get() == "foo" && name_reader.get() == "test_name", "Wrong values");
}

using TestArray = schema::Array<schema::UInt32>;

using TestArrayStruct = schema::Struct<
	schema::NamedMember<TestArray, "array">
>;

GIN_TEST("Message Array"){
	auto root = gin::heapMessageRoot<TestArray>();

	auto builder = root.build(3);

	auto one = builder.init(0);
	auto two = builder.init(1);
	auto three = builder.init(2);

	one.set(24);
	two.set(45);
	three.set(1230);

	auto reader = root.read();

	GIN_EXPECT(reader.get(0).get() == 24 && reader.get(1).get() == 45 && reader.get(2).get(), "Wrong values");
}
}
