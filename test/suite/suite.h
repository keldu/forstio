#pragma once

#include <string>
#include <memory>
#include <stdexcept>
#include <type_traits>

#include "common.h"

namespace gin {
namespace test {
class TestRunner;
class TestCase {
private:
	std::string file;
	uint line;
	std::string description;
	bool matched_filter;
	TestCase* next;
	TestCase** prev;	
	
	friend class TestRunner;
public:
	TestCase(const std::string& file_, uint line_, const std::string& description_);
	~TestCase();

	virtual void run() = 0;
};
}
}
#define GIN_TEST(description) \
	class GIN_UNIQUE_NAME(TestCase) : public ::gin::test::TestCase { \
	public: \
		GIN_UNIQUE_NAME(TestCase)(): ::gin::test::TestCase(__FILE__,__LINE__,description) {} \
		void run() override; \
	}GIN_UNIQUE_NAME(testCase); \
	void GIN_UNIQUE_NAME(TestCase)::run()

#define GIN_EXPECT(expr, msg_split) \
	if( ! (expr) ){ \
		auto msg = msg_split; \
		throw std::runtime_error{std::string{msg}};\
	}
