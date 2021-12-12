#include "suite/suite.h"

#include <cstdint>
#include <string>

#include "buffer.h"
#include "source/kelgin/message.h"
#include "source/kelgin/json.h"

#include "./data/json.h"

using gin::MessageList;
using gin::MessageStruct;
using gin::MessageStructMember;
using gin::MessagePrimitive;
using gin::heapMessageBuilder;

using gin::JsonCodec;
using gin::Error;

using gin::RingBuffer;

namespace schema {
using namespace gin::schema;
}

namespace {
using TestTuple = schema::Tuple<schema::UInt32, schema::String >;

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
using TestStruct = schema::Struct<
	schema::NamedMember<schema::UInt32, "test_uint">,
	schema::NamedMember<schema::String, "test_string">,
	schema::NamedMember<schema::String, "test_name">,
	schema::NamedMember<schema::Bool, "test_bool">
>;

GIN_TEST("JSON Struct Encoding"){
	auto builder = heapMessageBuilder();
	auto root = builder.initRoot<TestStruct>();
	
	auto uint = root.init<"test_uint">();
	uint.set(23);

	std::string test_string = "foo";
	auto string = root.init<"test_string">();
	string.set(test_string);
	
	auto string_name = root.init<"test_name">();
	string_name.set("test_name"_t.view());

	root.init<"test_bool">().set(false);

	JsonCodec codec;
	RingBuffer temp_buffer;
	codec.encode<TestStruct>(root.asReader(), temp_buffer);

	std::string expected_result{"{\"test_uint\":23,\"test_string\":\"foo\",\"test_name\":\"test_name\",\"test_bool\":false}"};
	
	std::string tmp_string = temp_buffer.toString();
	GIN_EXPECT(tmp_string == expected_result, std::string{"Bad encoding:\n"} + tmp_string);
}

using TestUnion = schema::Union<
	schema::NamedMember<schema::UInt32, "test_uint">,
	schema::NamedMember<schema::String, "test_string">
>;

GIN_TEST("JSON Union Encoding"){
	using namespace gin;
	{
		auto builder = heapMessageBuilder();
		auto root = builder.initRoot<TestUnion>();

		auto test_uint = root.init<"test_uint">();
		test_uint.set(23);

		RingBuffer buffer;
		JsonCodec codec;

		Error error = codec.encode<TestUnion>(root.asReader(), buffer);

		GIN_EXPECT(!error.failed(), error.message());
		
		std::string expected_result{"{\"test_uint\":23}"};
			
		std::string tmp_string = buffer.toString();
		GIN_EXPECT(tmp_string == expected_result, std::string{"Bad encoding:\n"} + tmp_string);
	}
	{
		auto builder = heapMessageBuilder();
		auto root = builder.initRoot<TestUnion>();

		auto test_string = root.init<"test_string">();
		test_string.set("foo");

		RingBuffer buffer;
		JsonCodec codec;

		Error error = codec.encode<TestUnion>(root.asReader(), buffer);

		GIN_EXPECT(!error.failed(), error.message());

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
	GIN_EXPECT( reader.get<"test_string">().get() == "banana", "Test String has wrong value" );
	GIN_EXPECT( reader.get<"test_uint">().get() == 5, "Test Unsigned has wrong value" );
	GIN_EXPECT( reader.get<"test_name">().get() == "keldu", "Test Name has wrong value" );
	GIN_EXPECT( reader.get<"test_bool">().get() == true, "Test Bool has wrong value" );
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

using TestStructDepth = schema::Struct<
	schema::NamedMember<schema::UInt32, "test_uint">,
	schema::NamedMember<TestStruct, "test_struct">,
	schema::NamedMember<schema::String, "test_name">
>;

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

	auto inner_reader = reader.get<"test_struct">();
	
	GIN_EXPECT( inner_reader.get<"test_string">().get() == "banana", "Test String has wrong value" );
	GIN_EXPECT( inner_reader.get<"test_uint">().get() == 40, "Test Unsigned has wrong value" );
	GIN_EXPECT( inner_reader.get<"test_name">().get() == "HaDiKo", "Test Name has wrong value" );
	GIN_EXPECT( inner_reader.get<"test_bool">().get() == false, "Test Bool has wrong value" );
	GIN_EXPECT( reader.get<"test_uint">().get() == 5, "Test Unsigned has wrong value" );
	GIN_EXPECT( reader.get<"test_name">().get() == "keldu", "Test Name has wrong value" );
}

using TestJsonOrgExample1 = schema::Struct<
	schema::NamedMember<
		schema::Struct<
			schema::NamedMember< schema::String, "title">,
			schema::NamedMember<
				schema::Struct<
					schema::NamedMember< schema::String, "title">,
					schema::NamedMember<
						schema::Struct<
							schema::NamedMember<
								schema::Struct<
									schema::NamedMember<schema::String, "ID">,
									schema::NamedMember<schema::String, "SortAs">,
									schema::NamedMember<schema::String, "GlossTerm">,
									schema::NamedMember<schema::String, "Acronym">,
									schema::NamedMember<schema::String, "Abbrev">,
									schema::NamedMember<
										schema::Struct<
											schema::NamedMember<schema::String, "para">,
											schema::NamedMember<
												schema::Tuple<
													schema::String,
													schema::String
												>,
												"GlossSeeAlso"
											>
										>,
										"GlossDef"
									>,
									schema::NamedMember<schema::String, "GlossSee">
								>,
								"GlossEntry"
							>
						>,
						"GlossList"	
					>
				>,
				"GlossDiv"
			>
		>,
		"glossary"
	>
>;

GIN_TEST ("JSON.org Decoding Example"){
	auto builder = heapMessageBuilder();
	TestJsonOrgExample::Builder root = builder.initRoot<TestJsonOrgExample1>();

	JsonCodec codec;
	RingBuffer temp_buffer;
	temp_buffer.push(*reinterpret_cast<const uint8_t*>(gin::json_org_example.data()), gin::json_org_example.size());

	Error error = codec.decode<TestJsonOrgExample>(root, temp_buffer);
	GIN_EXPECT(!error.failed(), error.message());

	auto reader = root.asReader();

	auto glossary_reader = reader.get<"glossary">();

	GIN_EXPECT(glossary_reader.get<"title">().get() == "example glossary", "Bad glossary title");

	auto gloss_div_reader = glossary_reader.get<"GlossDiv">();

	GIN_EXPECT(gloss_div_reader.get<"title">().get() == "S", "bad gloss div value" );

	auto gloss_list_reader = gloss_div_reader.get<"GlossList">();

	auto gloss_entry_reader = gloss_list_reader.get<"GlossEntry">();

	GIN_EXPECT(gloss_entry_reader.get<"ID">().get() == "SGML", "bad ID value" );
	GIN_EXPECT(gloss_entry_reader.get<"SortAs">().get() == "SGML", "bad SortAs value" );
	GIN_EXPECT(gloss_entry_reader.get<"GlossTerm">().get() == "Standard Generalized Markup Language", "bad GlossTerm value" );
	GIN_EXPECT(gloss_entry_reader.get<"Acronym">().get() == "SGML", "bad Acronym value" );
	GIN_EXPECT(gloss_entry_reader.get<"Abbrev">().get() == "ISO 8879:1986", "bad Abbrev value" );
	GIN_EXPECT(gloss_entry_reader.get<"GlossSee">().get() == "markup", "bad GlossSee value" );

	auto gloss_def_reader = gloss_entry_reader.get<"GlossDef">();

	GIN_EXPECT(gloss_def_reader.get<"para">().get() == "A meta-markup language, used to create markup languages such as DocBook.", "para value wrong");

	auto gloss_see_also_reader = gloss_def_reader.get<"GlossSeeAlso">();

	GIN_EXPECT(gloss_see_also_reader.get<0>().get() == "GML", "List 0 value wrong");
	GIN_EXPECT(gloss_see_also_reader.get<1>().get() == "XML", "List 1 value wrong");

	// (void) gloss_div_reader;
}
}
