#include "suite/suite.h"

#include "source/forstio/proto_kel.h"

#include <iostream>

namespace {
namespace schema {
	using namespace saw::schema;
}
using TestSize = schema::UInt32;

using TestTuple = schema::Tuple<schema::UInt32, schema::UInt16>;

using TestStruct = schema::Struct<
	schema::NamedMember<schema::UInt32, "test_uint">,
	schema::NamedMember<schema::String, "test_string">,
	schema::NamedMember<schema::String, "test_name">
>;

using TestUnion = schema::Union<
	schema::NamedMember<schema::UInt32, "test_uint">,
	schema::NamedMember<schema::String, "test_string">
>;

SAW_TEST("Primitive Encoding"){
	using namespace saw;
	uint32_t value = 5;

	auto root = heapMessageRoot<TestSize>();
	auto builder = root.build();

	builder.set(value);

	RingBuffer temp_buffer;
	ProtoKelCodec codec;

	Error error = codec.encode<TestSize>(root.read(), temp_buffer);

	SAW_EXPECT(!error.failed(), error.message());
	SAW_EXPECT(temp_buffer.readCompositeLength() == (sizeof(value)+sizeof(msg_packet_length_t)), "Bad Size: " + std::to_string(temp_buffer.readCompositeLength()));
	constexpr size_t pkt_shift = sizeof(msg_packet_length_t);
	SAW_EXPECT(temp_buffer[pkt_shift] == 5 && temp_buffer[pkt_shift+1] == 0 && temp_buffer[pkt_shift+2] == 0 && temp_buffer[pkt_shift+3] == 0, "Wrong encoded values");
}

SAW_TEST("List Encoding"){
	using namespace saw;

	auto root = heapMessageRoot<TestTuple>();
	auto builder = root.build();

	auto first = builder.init<0>();
	first.set(2135231);
	auto second = builder.init<1>();
	second.set(43871);

	RingBuffer buffer;
	ProtoKelCodec codec;

	Error error = codec.encode<TestTuple>(root.read(), buffer);

	SAW_EXPECT(!error.failed(), error.message());
	SAW_EXPECT(buffer.readCompositeLength() == 14, "Bad Size: " + std::to_string(buffer.readCompositeLength()));
	SAW_EXPECT("06 00 00 00\n00 00 00 00\nbf 94 20 00\n5f ab" == buffer.toHex(), "Not equal encoding\n"+buffer.toHex());
}

SAW_TEST("Struct Encoding"){
	using namespace saw;

	auto root = heapMessageRoot<TestStruct>();
	auto builder = root.build();

	auto test_uint = builder.init<"test_uint">();
	test_uint.set(23);

	std::string test_string = "foo";
	auto string = builder.init<"test_string">();
	string.set(test_string);
	
	auto string_name = builder.init<"test_name">();
	string_name.set("test_name");

	RingBuffer buffer;
	ProtoKelCodec codec;

	Error error = codec.encode<TestStruct>(builder.asReader(), buffer);

	SAW_EXPECT(!error.failed(), error.message());
	SAW_EXPECT(buffer.readCompositeLength() == 40, "Bad Size: " + std::to_string(buffer.readCompositeLength()));
	SAW_EXPECT("20 00 00 00\n00 00 00 00\n17 00 00 00\n03 00 00 00\n00 00 00 00\n66 6f 6f 09\n00 00 00 00\n00 00 00 74\n65 73 74 5f\n6e 61 6d 65"
		== buffer.toHex(), "Not equal encoding:\n"+buffer.toHex());
}

SAW_TEST("Union Encoding"){
	using namespace saw;
	{
		auto root = heapMessageRoot<TestUnion>();
		auto builder = root.build();

		auto test_uint = builder.init<"test_uint">();
		test_uint.set(23);

		RingBuffer buffer;
		ProtoKelCodec codec;

		Error error = codec.encode<TestUnion>(builder.asReader(), buffer);

		SAW_EXPECT(!error.failed(), error.message());
		SAW_EXPECT(buffer.readCompositeLength() == 16, "Bad Size: " + std::to_string(buffer.readCompositeLength()));
		SAW_EXPECT("08 00 00 00\n00 00 00 00\n00 00 00 00\n17 00 00 00"
			== buffer.toHex(), "Not equal encoding:\n"+buffer.toHex());
	}
	{
		auto root = heapMessageRoot<TestUnion>();
		auto builder = root.build();

		auto test_string = builder.init<"test_string">();
		test_string.set("foo");

		RingBuffer buffer;
		ProtoKelCodec codec;

		Error error = codec.encode<TestUnion>(builder.asReader(), buffer);

		SAW_EXPECT(!error.failed(), error.message());
		SAW_EXPECT(buffer.readCompositeLength() == 23, "Bad Size: " + std::to_string(buffer.readCompositeLength()));
		SAW_EXPECT("0f 00 00 00\n00 00 00 00\n01 00 00 00\n03 00 00 00\n00 00 00 00\n66 6f 6f"
			== buffer.toHex(), "Not equal encoding:\n"+buffer.toHex());
	}
}

SAW_TEST("Tuple Decoding"){
	using namespace saw;
	const uint8_t buffer_raw[] = {0x06, 0, 0, 0, 0, 0, 0, 0, 0xbf, 0x94, 0x20, 0x00, 0x5f, 0xab};

	RingBuffer buffer;
	buffer.push(*buffer_raw, sizeof(buffer_raw));

	ProtoKelCodec codec;

	auto root = heapMessageRoot<TestTuple>();
	auto builder = root.build();

	Error error = codec.decode<TestTuple>(builder, buffer);
	SAW_EXPECT(!error.failed(), error.message());
	
	auto reader = builder.asReader();

	auto first = reader.get<0>();
	auto second = reader.get<1>();

	SAW_EXPECT(first.get() == 2135231 && second.get() == 43871, "Values not correctly decoded");
}

SAW_TEST("Struct Decoding"){
	using namespace saw;
	const uint8_t buffer_raw[] = {0x20,0,0,0,0,0,0,0,0x17,0,0,0,0x03,0,0,0,0,0,0,0,0x66,0x6f,0x6f,0x09,0,0,0,0,0,0,0,0x74,0x65,0x73,0x74,0x5f,0x6e,0x61,0x6d,0x65};

	RingBuffer buffer;
	buffer.push(*buffer_raw, sizeof(buffer_raw));

	ProtoKelCodec codec;

	auto root = heapMessageRoot<TestStruct>();
	auto builder = root.build();

	Error error = codec.decode<TestStruct>(builder, buffer);
	auto reader = builder.asReader();

	auto foo_string = reader.get<"test_string">();
	auto test_uint = reader.get<"test_uint">();
	auto test_name = reader.get<"test_name">();

	SAW_EXPECT(!error.failed(), error.message());
	SAW_EXPECT(foo_string.get() == "foo" && test_uint.get() == 23 && test_name.get() == "test_name", "Values not correctly decoded");
}

SAW_TEST("Union Decoding"){
	using namespace saw;
	const uint8_t buffer_raw[] = {0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x66,0x6f,0x6f};

	RingBuffer buffer;
	buffer.push(*buffer_raw, sizeof(buffer_raw));

	ProtoKelCodec codec;

	auto root = heapMessageRoot<TestUnion>();
	auto builder = root.build();
	auto reader = builder.asReader();

	Error error = codec.decode<TestUnion>(builder, buffer);

	SAW_EXPECT(!error.failed(), error.message());
	SAW_EXPECT(reader.hasAlternative<"test_string">(), "Wrong union value");
	auto str_rd = reader.get<"test_string">();
	SAW_EXPECT(str_rd.get() == "foo", "Wrong value: " + std::string{str_rd.get()});
}

using TestArrayStruct = schema::Array<
	TestStruct
>;

SAW_TEST("Array Encoding"){
	using namespace saw;
	
	ProtoKelCodec codec;
	auto root = heapMessageRoot<TestArrayStruct>();
	auto builder = root.build(2);

	auto one = builder.init(0);
	auto two = builder.init(1);

	one.init<"test_uint">().set(4);
	one.init<"test_string">().set("foo");
	one.init<"test_name">().set("Fedor");
	
	two.init<"test_uint">().set(9);
	two.init<"test_string">().set("bar");
	two.init<"test_name">().set("Bravo");

	RingBuffer buffer;

	Error error = codec.encode<TestArrayStruct>(root.read(), buffer);

	SAW_EXPECT(!error.failed(), "Error occured");
}
}
