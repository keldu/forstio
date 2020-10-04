#include "suite/suite.h"

#include "source/proto_kel.h"

namespace {
typedef gin::MessagePrimitive<size_t> TestSize;

GIN_TEST("Primitive Encoding"){
	using namespace gin;
	uint32_t value = 5;

	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestSize>();

	root.set(value);

	RingBuffer temp_buffer;

	Error error = ProtoKelEncodeImpl<TestSize>::encode(root.asReader(), temp_buffer);

	GIN_EXPECT(!error.failed(), "Error: " + error.message());
	GIN_EXPECT(temp_buffer.size() == sizeof(value), "Bad Size: " + std::to_string(temp_buffer.size()));
	GIN_EXPECT(temp_buffer[0] == 5 && temp_buffer[1] == 0 && temp_buffer[2] == 0 && temp_buffer[3] == 0, "Wrong encoded values");
}
}