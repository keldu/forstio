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

typedef gin::MessageUnion<
	gin::MessageUnionMember<gin::MessagePrimitive<uint32_t>, decltype("test_uint"_t)>,
	gin::MessageUnionMember<gin::MessagePrimitive<std::string>, decltype("test_string"_t)>
> TestUnion;

GIN_TEST("Primitive Encoding"){
	using namespace gin;
	uint32_t value = 5;

	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestSize>();

	root.set(value);

	RingBuffer temp_buffer;

	Error error = ProtoKelEncodeImpl<TestSize>::encode(root.asReader(), temp_buffer);

	GIN_EXPECT(!error.failed(), error.message());
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

	GIN_EXPECT(!error.failed(), error.message());
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

	GIN_EXPECT(!error.failed(), error.message());
	GIN_EXPECT(buffer.readCompositeLength() == 40, "Bad Size: " + std::to_string(buffer.readCompositeLength()));
	GIN_EXPECT("20 00 00 00\n00 00 00 00\n17 00 00 00\n03 00 00 00\n00 00 00 00\n66 6f 6f 09\n00 00 00 00\n00 00 00 74\n65 73 74 5f\n6e 61 6d 65"
		== buffer.toHex(), "Not equal encoding:\n"+buffer.toHex());
}

GIN_TEST("Union Encoding"){
	using namespace gin;
	{
		auto builder = heapMessageBuilder();
		auto root = builder.initRoot<TestUnion>();

		auto test_uint = root.init<decltype("test_uint"_t)>();
		test_uint.set(23);

		RingBuffer buffer;
		ProtoKelCodec codec;

		Error error = codec.encode<TestUnion>(root.asReader(), buffer);

		GIN_EXPECT(!error.failed(), error.message());
		GIN_EXPECT(buffer.readCompositeLength() == 16, "Bad Size: " + std::to_string(buffer.readCompositeLength()));
		GIN_EXPECT("08 00 00 00\n00 00 00 00\n00 00 00 00\n17 00 00 00"
			== buffer.toHex(), "Not equal encoding:\n"+buffer.toHex());
	}
	{
		auto builder = heapMessageBuilder();
		auto root = builder.initRoot<TestUnion>();

		auto test_string = root.init<decltype("test_string"_t)>();
		test_string.set("foo");

		RingBuffer buffer;
		ProtoKelCodec codec;

		Error error = codec.encode<TestUnion>(root.asReader(), buffer);

		GIN_EXPECT(!error.failed(), error.message());
		GIN_EXPECT(buffer.readCompositeLength() == 23, "Bad Size: " + std::to_string(buffer.readCompositeLength()));
		GIN_EXPECT("0f 00 00 00\n00 00 00 00\n01 00 00 00\n03 00 00 00\n00 00 00 00\n66 6f 6f"
			== buffer.toHex(), "Not equal encoding:\n"+buffer.toHex());
	}
}

GIN_TEST("List Decoding"){
	using namespace gin;
	const uint8_t buffer_raw[] = {0x06, 0, 0, 0, 0, 0, 0, 0, 0xbf, 0x94, 0x20, 0x00, 0x5f, 0xab};

	RingBuffer buffer;
	buffer.push(*buffer_raw, sizeof(buffer_raw));

	ProtoKelCodec codec;

	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestList>();

	Error error = codec.decode<TestList>(root, buffer);
	GIN_EXPECT(!error.failed(), error.message());
	
	auto reader = root.asReader();

	auto first = reader.get<0>();
	auto second = reader.get<1>();

	GIN_EXPECT(first.get() == 2135231 && second.get() == 43871, "Values not correctly decoded");
}

GIN_TEST("Struct Decoding"){
	using namespace gin;
	const uint8_t buffer_raw[] = {0x20,0,0,0,0,0,0,0,0x17,0,0,0,0x03,0,0,0,0,0,0,0,0x66,0x6f,0x6f,0x09,0,0,0,0,0,0,0,0x74,0x65,0x73,0x74,0x5f,0x6e,0x61,0x6d,0x65};

	RingBuffer buffer;
	buffer.push(*buffer_raw, sizeof(buffer_raw));

	ProtoKelCodec codec;

	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestStruct>();

	Error error = codec.decode<TestStruct>(root, buffer);
	auto reader = root.asReader();

	auto foo_string = reader.get<decltype("test_string"_t)>();
	auto test_uint = reader.get<decltype("test_uint"_t)>();
	auto test_name = reader.get<decltype("test_name"_t)>();

	GIN_EXPECT(!error.failed(), error.message());
	GIN_EXPECT(foo_string.get() == "foo" && test_uint.get() == 23 && test_name.get() == "test_name", "Values not correctly decoded");
}

GIN_TEST("Union Decoding"){
	using namespace gin;
	const uint8_t buffer_raw[] = {0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x66,0x6f,0x6f};

	RingBuffer buffer;
	buffer.push(*buffer_raw, sizeof(buffer_raw));

	ProtoKelCodec codec;

	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestUnion>();
	auto reader = root.asReader();

	Error error = codec.decode<TestUnion>(root, buffer);

	GIN_EXPECT(!error.failed(), error.message());
	GIN_EXPECT(reader.holdsAlternative<decltype("test_string"_t)>(), "Wrong union value");
	auto str_rd = reader.get<decltype("test_string"_t)>();
	GIN_EXPECT(str_rd.get() == "foo", "Wrong value: " + std::string{str_rd.get()});
}
}
