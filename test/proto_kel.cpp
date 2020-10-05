#include "suite/suite.h"

#include "source/proto_kel.h"

#include <iostream>

namespace {
typedef gin::MessagePrimitive<uint32_t> TestSize;

typedef gin::MessageList<gin::MessagePrimitive<uint32_t>, gin::MessagePrimitive<uint16_t>> TestList;

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
	GIN_EXPECT("0600000000000000bf9420005fab" == buffer.toHex(), "Not equal encoding");
}

GIN_TEST("Struct Encoding"){
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
	GIN_EXPECT("0600000000000000bf9420005fab" == buffer.toHex(), "Not equal encoding");
}
}