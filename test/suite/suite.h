#pragma once

#include <string>
#include <memory>
#include <stdexcept>

#define KEL_CONCAT_(x,y) x##y
#define KEL_CONCAT(x,y) KEL_CONCAT_(x,y)
#define KEL_UNIQUE_NAME(prefix) KEL_CONCAT(prefix, __LINE__)

namespace ent {
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
#define KEL_TEST(description) \
	class KEL_UNIQUE_NAME(TestCase) : public ::ent::test::TestCase { \
	public: \
		KEL_UNIQUE_NAME(TestCase)(): ::ent::test::TestCase(__FILE__,__LINE__,description) {} \
		void run() override; \
	}KEL_UNIQUE_NAME(testCase); \
	void KEL_UNIQUE_NAME(TestCase)::run()

#define KEL_EXPECT(expr, msg) \
	if( ! (expr) ){ \
		throw std::runtime_error{msg}; \
	}
