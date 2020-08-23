#include "suite/suite.h"

#include "async.h"


GIN_TEST("Async Adapt"){
	using namespace gin;

	EventLoop event_loop;
	WaitScope wait_scope{event_loop};

	auto feeder_conveyor = newConveyorAndFeeder<size_t>();

	feeder_conveyor.feeder->feed(5);

	ErrorOr<size_t> foo = feeder_conveyor.conveyor.take();

	GIN_EXPECT(!foo.isError(), "Return is an error: " + foo.error().message());
	GIN_EXPECT(foo.isValue(), "Return is not a value");
	GIN_EXPECT(foo.value() == 5, "Values not 5, but " + std::to_string(foo.value()));
}


GIN_TEST("Async Adapt Multiple"){
	using namespace gin;

	EventLoop event_loop;
	WaitScope wait_scope{event_loop};

	auto feeder_conveyor = newConveyorAndFeeder<size_t>();

	feeder_conveyor.feeder->feed(5);

	ErrorOr<size_t> foo = feeder_conveyor.conveyor.take();

	GIN_EXPECT(!foo.isError(), "Return is an error: " + foo.error().message());
	GIN_EXPECT(foo.isValue(), "Return is not a value");
	GIN_EXPECT(foo.value() == 5, "Values not 5, but " + std::to_string(foo.value()));

	feeder_conveyor.feeder->feed(10);

	ErrorOr<size_t> bar = feeder_conveyor.conveyor.take();

	GIN_EXPECT(!bar.isError(), "Return is an error: " + bar.error().message());
	GIN_EXPECT(bar.isValue(), "Return is not a value");
	GIN_EXPECT(bar.value() == 10, "Values not 10, but " + std::to_string(bar.value()));

	feeder_conveyor.feeder->feed(2);
	feeder_conveyor.feeder->feed(4234);

	ErrorOr<size_t> a = feeder_conveyor.conveyor.take();
	ErrorOr<size_t> b = feeder_conveyor.conveyor.take();

	GIN_EXPECT(!a.isError(), "Return is an error: " + a.error().message());
	GIN_EXPECT(a.isValue(), "Return is not a value");
	GIN_EXPECT(a.value() == 2, "Values not 2, but " + std::to_string(a.value()));

	GIN_EXPECT(!b.isError(), "Return is an error: " + b.error().message());
	GIN_EXPECT(b.isValue(), "Return is not a value");
	GIN_EXPECT(b.value() == 4234, "Values not 4234, but " + std::to_string(b.value()));
}

GIN_TEST("Async Conversion"){
	using namespace gin;

	EventLoop event_loop;
	WaitScope wait_scope{event_loop};

	auto feeder_conveyor = newConveyorAndFeeder<size_t>();

	Conveyor<std::string> string_conveyor = feeder_conveyor.conveyor.then([](size_t foo){
		return std::to_string(foo);
	});

	feeder_conveyor.feeder->feed(10);

	ErrorOr<std::string> foo = string_conveyor.take();

	GIN_EXPECT(!foo.isError(), "Return is an error: " + foo.error().message());
	GIN_EXPECT(foo.isValue(), "Return is not a value");
	GIN_EXPECT(foo.value() == std::to_string(10), "Values is not 10, but " + foo.value());
}

GIN_TEST("Async Conversion Multistep"){
	using namespace gin;

	EventLoop event_loop;
	WaitScope wait_scope{event_loop};

	auto feeder_conveyor = newConveyorAndFeeder<size_t>();

	Conveyor<bool> conveyor = feeder_conveyor.conveyor.then([](size_t foo){
		return std::to_string(foo);
	}).then([](const std::string& value){
		return value != "10";
	}).then([](bool value){
		return !value;
	});

	feeder_conveyor.feeder->feed(10);

	ErrorOr<bool> foo = conveyor.take();

	GIN_EXPECT(!foo.isError(), "Return is an error: " + foo.error().message());
	GIN_EXPECT(foo.isValue(), "Return is not a value");
	GIN_EXPECT(foo.value(), "Values is not true");
}

GIN_TEST("Async Scheduling"){
	using namespace gin;

	EventLoop event_loop;
	WaitScope wait_scope{event_loop};

	auto feeder_conveyor = newConveyorAndFeeder<size_t>();

	/*
	* Attach node test data
	*/
	Own<size_t> counter = heap<size_t>();
	size_t* ctr_ptr = counter.get();
	*ctr_ptr = 0;

	Conveyor<std::string> string_conveyor = feeder_conveyor.conveyor
	.then([ctr_ptr](size_t foo){
		return std::to_string(foo + ++(*ctr_ptr));
	})
	.attach(std::move(counter))
	.buffer(10)
	.then([](const std::string& value){
		return value + std::string{"post"};
	})
	.buffer(10)
	.then([](const std::string& value){
		return std::string{"pre"} + value;
	})
	.buffer(10);

	feeder_conveyor.feeder->feed(10);
	feeder_conveyor.feeder->feed(20);
	feeder_conveyor.feeder->feed(30);

	wait_scope.poll();

	ErrorOr<std::string> foo = string_conveyor.take();

	GIN_EXPECT(!foo.isError(), "Return is an error: " + foo.error().message());
	GIN_EXPECT(foo.isValue(), "Return is not a value");
	GIN_EXPECT(foo.value() == (std::string{"pre"} + std::to_string(11) + std::string{"post"}), "Values is not pre11post, but " + foo.value());

	ErrorOr<std::string> foo_20 = string_conveyor.take();

	GIN_EXPECT(!foo_20.isError(), "Return is an error: " + foo_20.error().message());
	GIN_EXPECT(foo_20.isValue(), "Return is not a value");
	GIN_EXPECT(foo_20.value() == (std::string{"pre"} + std::to_string(22) + std::string{"post"}), "Values is not pre22post, but " + foo_20.value());

	ErrorOr<std::string> foo_30 = string_conveyor.take();

	GIN_EXPECT(!foo_30.isError(), "Return is an error: " + foo_30.error().message());
	GIN_EXPECT(foo_30.isValue(), "Return is not a value");
	GIN_EXPECT(foo_30.value() == (std::string{"pre"} + std::to_string(33) + std::string{"post"}), "Values is not pre33post, but " + foo_30.value());
}