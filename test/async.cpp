#include "suite/suite.h"

#include "async.h"

using namespace gin;

GIN_TEST("Async"){
	EventLoop event_loop;

	WaitScope wait_scope{event_loop};

	wait_scope.wait(std::chrono::seconds{1});
}