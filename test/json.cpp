#include "suite/suite.h"

#include <cstdint>
#include <string>

#include "buffer.h"
#include "source/message.h"
#include "source/json.h"

#include "./data/json.h"

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
	MessageStructMember<MessagePrimitive<std::string>, decltype("test_name"_t)>,
	MessageStructMember<MessagePrimitive<bool>, decltype("test_bool"_t)>
> TestStruct;

GIN_TEST("JSON Struct Encoding"){
	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestStruct>();
	
	auto uint = root.init<decltype("test_uint"_t)>();
	uint.set(23);

	std::string test_string = "foo";
	auto string = root.init<decltype("test_string"_t)>();
	string.set(test_string);
	
	auto string_name = root.init<decltype("test_name"_t)>();
	string_name.set("test_name"_t.view());

	root.init<decltype("test_bool"_t)>().set(false);

	JsonCodec codec;
	RingBuffer temp_buffer;
	codec.encode<TestStruct>(root.asReader(), temp_buffer);

	std::string expected_result{"{\"test_uint\":23,\"test_string\":\"foo\",\"test_name\":\"test_name\",\"test_bool\":false}"};
	
	std::string tmp_string = temp_buffer.toString();
	GIN_EXPECT(tmp_string == expected_result, std::string{"Bad encoding:\n"} + tmp_string);
}

typedef gin::MessageUnion<
	gin::MessageUnionMember<gin::MessagePrimitive<uint32_t>, decltype("test_uint"_t)>,
	gin::MessageUnionMember<gin::MessagePrimitive<std::string>, decltype("test_string"_t)>
> TestUnion;

GIN_TEST("JSON Union Encoding"){
	using namespace gin;
	{
		auto builder = heapMessageBuilder();
		auto root = builder.initRoot<TestUnion>();

		auto test_uint = root.init<decltype("test_uint"_t)>();
		test_uint.set(23);

		RingBuffer buffer;
		JsonCodec codec;

		Error error = codec.encode<TestUnion>(root.asReader(), buffer);

		GIN_EXPECT(!error.failed(), "Error: " + error.message());
		
		std::string expected_result{"{\"test_uint\":23}"};
			
		std::string tmp_string = buffer.toString();
		GIN_EXPECT(tmp_string == expected_result, std::string{"Bad encoding:\n"} + tmp_string);
	}
	{
		auto builder = heapMessageBuilder();
		auto root = builder.initRoot<TestUnion>();

		auto test_string = root.init<decltype("test_string"_t)>();
		test_string.set("foo");

		RingBuffer buffer;
		JsonCodec codec;

		Error error = codec.encode<TestUnion>(root.asReader(), buffer);

		GIN_EXPECT(!error.failed(), "Error: " + error.message());

		std::string expected_result{"{\"test_string\":\"foo\"}"};
			
		std::string tmp_string = buffer.toString();
		GIN_EXPECT(tmp_string == expected_result, std::string{"Bad encoding:\n"} + tmp_string);
	}
}

GIN_TEST("JSON Struct Decoding"){
	std::string json_string = R"(
	{
		"test_string" :"banana" ,
		"test_uint" : 5,
		"test_name" : "keldu",
		"test_bool" : true
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
	GIN_EXPECT( reader.get<decltype("test_bool"_t)>().get() == true, "Test Bool has wrong value" );
}

GIN_TEST("JSON List Decoding"){
	std::string json_string = R"(
	[
		12, 
			"free"
	])";

	auto builder = heapMessageBuilder();
	TestList::Builder root = builder.initRoot<TestList>();

	JsonCodec codec;
	RingBuffer temp_buffer;
	temp_buffer.push(*reinterpret_cast<const uint8_t*>(json_string.data()), json_string.size());
	Error error = codec.decode<TestList>(root, temp_buffer);
	GIN_EXPECT( !error.failed(), error.message() );

	auto reader = root.asReader();
	GIN_EXPECT( reader.get<0>().get() == 12, "Test Unsigned has wrong value" );
	GIN_EXPECT( reader.get<1>().get() == "free", "Test String has wrong value" );
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
			"test_name":"HaDiKo",
			"test_bool" :false
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
	GIN_EXPECT( inner_reader.get<decltype("test_bool"_t)>().get() == false, "Test Bool has wrong value" );
	GIN_EXPECT( reader.get<decltype("test_uint"_t)>().get() == 5, "Test Unsigned has wrong value" );
	GIN_EXPECT( reader.get<decltype("test_name"_t)>().get() == "keldu", "Test Name has wrong value" );
}

typedef MessageStruct<
	MessageStructMember<
		MessageStruct<
			MessageStructMember< MessagePrimitive<std::string>, decltype("title"_t)>,
			MessageStructMember<
				MessageStruct<
					MessageStructMember<MessagePrimitive<std::string>,decltype("title"_t)>,
					MessageStructMember<
						MessageStruct<
							MessageStructMember<
								MessageStruct<
									MessageStructMember<MessagePrimitive<std::string>,decltype("ID"_t)>,
									MessageStructMember<MessagePrimitive<std::string>,decltype("SortAs"_t)>,
									MessageStructMember<MessagePrimitive<std::string>,decltype("GlossTerm"_t)>,
									MessageStructMember<MessagePrimitive<std::string>,decltype("Acronym"_t)>,
									MessageStructMember<MessagePrimitive<std::string>,decltype("Abbrev"_t)>,
									MessageStructMember<
										MessageStruct<
											MessageStructMember<MessagePrimitive<std::string>, decltype("para"_t)>,
											MessageStructMember<
												MessageList<
													MessagePrimitive<std::string>,
													MessagePrimitive<std::string>
												>
											, decltype("GlossSeeAlso"_t)>
										>
									, decltype("GlossDef"_t)>,
									MessageStructMember<MessagePrimitive<std::string>, decltype("GlossSee"_t)>
								>
							, decltype("GlossEntry"_t)>
						>
					, decltype("GlossList"_t)>
				>
			, decltype("GlossDiv"_t)>
		>
	, decltype("glossary"_t)>
> TestJsonOrgExample;

GIN_TEST ("JSON.org Decoding Example"){
	auto builder = heapMessageBuilder();
	TestJsonOrgExample::Builder root = builder.initRoot<TestJsonOrgExample>();

	JsonCodec codec;
	RingBuffer temp_buffer;
	temp_buffer.push(*reinterpret_cast<const uint8_t*>(gin::json_org_example.data()), gin::json_org_example.size());

	Error error = codec.decode<TestJsonOrgExample>(root, temp_buffer);
	GIN_EXPECT(!error.failed(), error.message());

	auto reader = root.asReader();

	auto glossary_reader = reader.get<decltype("glossary"_t)>();

	GIN_EXPECT(glossary_reader.get<decltype("title"_t)>().get() == "example glossary", "Bad glossary title");

	auto gloss_div_reader = glossary_reader.get<decltype("GlossDiv"_t)>();

	GIN_EXPECT(gloss_div_reader.get<decltype("title"_t)>().get() == "S", "bad gloss div value" );

	auto gloss_list_reader = gloss_div_reader.get<decltype("GlossList"_t)>();

	auto gloss_entry_reader = gloss_list_reader.get<decltype("GlossEntry"_t)>();

	GIN_EXPECT(gloss_entry_reader.get<decltype("ID"_t)>().get() == "SGML", "bad ID value" );
	GIN_EXPECT(gloss_entry_reader.get<decltype("SortAs"_t)>().get() == "SGML", "bad SortAs value" );
	GIN_EXPECT(gloss_entry_reader.get<decltype("GlossTerm"_t)>().get() == "Standard Generalized Markup Language", "bad GlossTerm value" );
	GIN_EXPECT(gloss_entry_reader.get<decltype("Acronym"_t)>().get() == "SGML", "bad Acronym value" );
	GIN_EXPECT(gloss_entry_reader.get<decltype("Abbrev"_t)>().get() == "ISO 8879:1986", "bad Abbrev value" );
	GIN_EXPECT(gloss_entry_reader.get<decltype("GlossSee"_t)>().get() == "markup", "bad GlossSee value" );

	auto gloss_def_reader = gloss_entry_reader.get<decltype("GlossDef"_t)>();

	GIN_EXPECT(gloss_def_reader.get<decltype("para"_t)>().get() == "A meta-markup language, used to create markup languages such as DocBook.", "para value wrong");

	auto gloss_see_also_reader = gloss_def_reader.get<decltype("GlossSeeAlso"_t)>();

	GIN_EXPECT(gloss_see_also_reader.get<0>().get() == "GML", "List 0 value wrong");
	GIN_EXPECT(gloss_see_also_reader.get<1>().get() == "XML", "List 1 value wrong");

	// (void) gloss_div_reader;
}
}