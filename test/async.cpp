#include "suite/suite.h"

#include "async.h"


GIN_TEST("Async"){
	using namespace gin;

	EventLoop event_loop;
	WaitScope wait_scope{event_loop};

	wait_scope.wait(std::chrono::seconds{1});

	auto feeder_conveyor = newConveyorAndFeeder<size_t>();

	feeder_conveyor.feeder->feed(5);

	ErrorOr<size_t> foo = feeder_conveyor.conveyor.take();

	GIN_EXPECT(foo.isValue() && foo.value() == 5, "Values not 5");
}