#include "suite/suite.h"

#include <cstdint>
#include <string>

#include "buffer.h"
#include "source/message.h"
#include "source/json.h"

using gin::MessageList;
using gin::MessageStruct;
using gin::MessageStructMember;
using gin::MessagePrimitive;
using gin::heapMessageBuilder;

using gin::JsonCodec;
using gin::Error;

using gin::RingBuffer;

namespace {
typedef MessageList<MessagePrimitive<uint32_t>, MessagePrimitive<std::string> > TestList;

GIN_TEST("JSON List Encoding"){
	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestList>();
	auto uint = root.init<0>();
	uint.set(12);
	auto string = root.init<1>();
	string.set("free");

	RingBuffer temp_buffer;
	JsonCodec codec;
	codec.encode<TestList>(root.asReader(), temp_buffer);

	std::string tmp_string = temp_buffer.toString();
	GIN_EXPECT(tmp_string == "[12,\"free\"]", std::string{"Bad encoding:\n"} + tmp_string);
}

typedef MessageStruct<
	MessageStructMember<MessagePrimitive<uint32_t>, decltype("test_uint"_t)>,
	MessageStructMember<MessagePrimitive<std::string>, decltype("test_string"_t)>,
	MessageStructMember<MessagePrimitive<std::string>, decltype("test_name"_t)>
> TestStruct;

GIN_TEST("JSON Struct Encoding"){
	std::string test_string = "foo";
	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestStruct>();
	auto uint = root.init<decltype("test_uint"_t)>();
	uint.set(23);
	auto string = root.init<decltype("test_string"_t)>();
	string.set(test_string);
	auto string_name = root.init<decltype("test_name"_t)>();
	string_name.set("test_name"_t.view());

	RingBuffer temp_buffer;
	JsonCodec codec;
	codec.encode<TestStruct>(root.asReader(), temp_buffer);

	std::string expected_result{"{\"test_uint\":23,\"test_string\":\"foo\",\"test_name\":\"test_name\"}"};
	
	std::string tmp_string = temp_buffer.toString();
	GIN_EXPECT(tmp_string == expected_result, std::string{"Bad encoding:\n"} + tmp_string);
}

GIN_TEST("JSON Struct Decoding"){
	std::string json_string = R"(
	{
		"test_string" :"banana" ,
		"test_uint" : 5,
		"test_name" : "keldu"
	})";

	auto builder = heapMessageBuilder();
	TestStruct::Builder root = builder.initRoot<TestStruct>();

	JsonCodec codec;
	RingBuffer temp_buffer;
	temp_buffer.push(*reinterpret_cast<const uint8_t*>(json_string.data()), json_string.size());
	Error error = codec.decode<TestStruct>(root, temp_buffer);
	GIN_EXPECT( !error.failed(), error.message() );

	auto reader = root.asReader();
	GIN_EXPECT( reader.get<decltype("test_string"_t)>().get() == "banana", "Test String has wrong value" );
	GIN_EXPECT( reader.get<decltype("test_uint"_t)>().get() == 5, "Test Unsigned has wrong value" );
	GIN_EXPECT( reader.get<decltype("test_name"_t)>().get() == "keldu", "Test Name has wrong value" );
}

typedef MessageStruct<
	MessageStructMember<MessagePrimitive<uint32_t>, decltype("test_uint"_t)>,
	MessageStructMember<TestStruct, decltype("test_struct"_t)>,
	MessageStructMember<MessagePrimitive<std::string>, decltype("test_name"_t)>
> TestStructDepth;

GIN_TEST("JSON Struct Decoding Two layer"){
	std::string json_string = R"(
	{
		"test_struct" :{
			"test_string" : 	"banana",
			"test_uint": 40,
			"test_name":"HaDiKo"
		},
		"test_uint": 5,
		"test_name" : "keldu"
	})";

	auto builder = heapMessageBuilder();
	TestStructDepth::Builder root = builder.initRoot<TestStructDepth>();

	JsonCodec codec;
	RingBuffer temp_buffer;
	temp_buffer.push(*reinterpret_cast<const uint8_t*>(json_string.data()), json_string.size());
	Error error = codec.decode<TestStructDepth>(root, temp_buffer);
	GIN_EXPECT( !error.failed(), error.message() );

	auto reader = root.asReader();

	auto inner_reader = reader.get<decltype("test_struct"_t)>();
	
	GIN_EXPECT( inner_reader.get<decltype("test_string"_t)>().get() == "banana", "Test String has wrong value" );
	GIN_EXPECT( inner_reader.get<decltype("test_uint"_t)>().get() == 40, "Test Unsigned has wrong value" );
	GIN_EXPECT( inner_reader.get<decltype("test_name"_t)>().get() == "HaDiKo", "Test Name has wrong value" );
	GIN_EXPECT( reader.get<decltype("test_uint"_t)>().get() == 5, "Test Unsigned has wrong value" );
	GIN_EXPECT( reader.get<decltype("test_name"_t)>().get() == "keldu", "Test Name has wrong value" );
}
}