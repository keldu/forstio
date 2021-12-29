#pragma once

#include <string>
#include <memory>
#include <stdexcept>
#include <type_traits>

#include "common.h"

namespace saw {
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
#define SAW_TEST(description) \
	class SAW_UNIQUE_NAME(TestCase) : public ::saw::test::TestCase { \
	public: \
		SAW_UNIQUE_NAME(TestCase)(): ::saw::test::TestCase(__FILE__,__LINE__,description) {} \
		void run() override; \
	}SAW_UNIQUE_NAME(testCase); \
	void SAW_UNIQUE_NAME(TestCase)::run()

#define SAW_EXPECT(expr, msg_split) \
	if( ! (expr) ){ \
		auto msg = msg_split; \
		throw std::runtime_error{std::string{msg}};\
	}
