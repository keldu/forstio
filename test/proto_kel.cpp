#include "suite/suite.h"

#include "source/proto_kel.h"

#include <iostream>

namespace {
typedef gin::MessagePrimitive<uint32_t> TestSize;

typedef gin::MessageList<gin::MessagePrimitive<uint32_t>, gin::MessagePrimitive<uint16_t>> TestList;

typedef gin::MessageStruct<
	gin::MessageStructMember<gin::MessagePrimitive<uint32_t>, decltype("test_uint"_t)>,
	gin::MessageStructMember<gin::MessagePrimitive<std::string>, decltype("test_string"_t)>,
	gin::MessageStructMember<gin::MessagePrimitive<std::string>, decltype("test_name"_t)>
> TestStruct;

GIN_TEST("Primitive Encoding"){
	using namespace gin;
	uint32_t value = 5;

	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestSize>();

	root.set(value);

	RingBuffer temp_buffer;

	Error error = ProtoKelEncodeImpl<TestSize>::encode(root.asReader(), temp_buffer);

	GIN_EXPECT(!error.failed(), "Error: " + error.message());
	GIN_EXPECT(temp_buffer.readCompositeLength() == sizeof(value), "Bad Size: " + std::to_string(temp_buffer.readCompositeLength()));
	GIN_EXPECT(temp_buffer[0] == 5 && temp_buffer[1] == 0 && temp_buffer[2] == 0 && temp_buffer[3] == 0, "Wrong encoded values");
}

GIN_TEST("List Encoding"){
	using namespace gin;

	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestList>();

	auto first = root.init<0>();
	first.set(2135231);
	auto second = root.init<1>();
	second.set(43871);

	RingBuffer buffer;
	ProtoKelCodec codec;

	Error error = codec.encode<TestList>(root.asReader(), buffer);

	GIN_EXPECT(!error.failed(), "Error: " + error.message());
	GIN_EXPECT(buffer.readCompositeLength() == 14, "Bad Size: " + std::to_string(buffer.readCompositeLength()));
	GIN_EXPECT("06 00 00 00\n00 00 00 00\nbf 94 20 00\n5f ab" == buffer.toHex(), "Not equal encoding\n"+buffer.toHex());
}

GIN_TEST("Struct Encoding"){
	using namespace gin;

	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestStruct>();

	auto test_uint = root.init<decltype("test_uint"_t)>();
	test_uint.set(23);

	std::string test_string = "foo";
	auto string = root.init<decltype("test_string"_t)>();
	string.set(test_string);
	
	auto string_name = root.init<decltype("test_name"_t)>();
	string_name.set("test_name"_t.view());

	RingBuffer buffer;
	ProtoKelCodec codec;

	Error error = codec.encode<TestStruct>(root.asReader(), buffer);

	GIN_EXPECT(!error.failed(), "Error: " + error.message());
	GIN_EXPECT(buffer.readCompositeLength() == 40, "Bad Size: " + std::to_string(buffer.readCompositeLength()));
	GIN_EXPECT("20 00 00 00\n00 00 00 00\n17 00 00 00\n03 00 00 00\n00 00 00 00\n66 6f 6f 09\n00 00 00 00\n00 00 00 74\n65 73 74 5f\n6e 61 6d 65"
		== buffer.toHex(), "Not equal encoding:\n"+buffer.toHex());
}
}