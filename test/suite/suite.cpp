#include "suite.h"

#include <map>
#include <string>
#include <chrono>
#include <iostream>

namespace saw {
namespace test {

	TestCase* testCaseHead = nullptr;
	TestCase** testCaseTail = &testCaseHead;

	TestCase::TestCase(const std::string& file_, uint line_, const std::string& description_):
		file{file_},
		line{line_},
		description{description_},
		matched_filter{false},
		next{nullptr},
		prev{testCaseTail}
	{
		*prev = this;
		testCaseTail = &next;
	}

	TestCase::~TestCase(){
		*prev = next;
		if( next == nullptr ){
			testCaseTail = prev;
		}else{
			next->prev = prev;
		}
	}

	class TestRunner {
	private:
		enum Colour {
			RED,
			GREEN,
			BLUE,
			WHITE
		};

		void write(Colour colour, const std::string& front, const std::string& message){
			std::string start_col, end_col;
			switch(colour){
				case RED: start_col = "\033[0;1;31m"; break;
				case GREEN: start_col = "\033[0;1;32m"; break;
				case BLUE: start_col = "\033[0;1;34m"; break;
				case WHITE: start_col = "\033[0m"; break;
			}
			end_col = "\033[0m";
			std::cout<<start_col<<front<<end_col<<message<<'\n';
		}	
	public:
		void allowAll(){
			for(TestCase* testCase = testCaseHead; testCase != nullptr; testCase = testCase->next){
				testCase->matched_filter = true;
			}
		}

		int run() {
			size_t passed_count = 0;
			size_t failed_count = 0;
			
			for(TestCase* testCase = testCaseHead; testCase != nullptr; testCase =testCase->next){
				if(testCase->matched_filter){
					std::string name = testCase->file + std::string(":") + std::to_string(testCase->line) + std::string(": ") + testCase->description;
					write(BLUE, "[ TEST ] ", name);
					bool failed = true;
					std::string fail_message;
					auto start_clock = std::chrono::steady_clock::now();
					try {
						testCase->run();
						failed = false;
					}catch(std::exception& e){
						fail_message = e.what();
						failed = true;
					}
					auto stop_clock = std::chrono::steady_clock::now();

					auto runtime_duration_intern = stop_clock - start_clock;
					auto runtime_duration = std::chrono::duration_cast<std::chrono::microseconds>(runtime_duration_intern);
					std::string message = name + std::string(" (") + std::to_string(runtime_duration.count()) + std::string(" µs)");
					if(failed){
						write(RED, "[ FAIL ] ", message + " " + fail_message);
						++failed_count;
					}else{
						write(GREEN, "[ PASS ] ", message);
						++passed_count;
					}
				}
			}

			if(passed_count>0) write(GREEN, std::to_string(passed_count) + std::string(" test(s) passed"), "");
			if(failed_count>0) write(RED, std::to_string(failed_count) + std::string(" test(s) failed"), "");
			return -failed_count;
		}
	};
}
}

#if SAW_COMPILE_TEST_BINARY

int main() {
	saw::test::TestRunner runner;
	runner.allowAll();
	int rv = runner.run();
	return rv<0?-1:0;
}

#endif