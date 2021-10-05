#include "suite/suite.h"

#include "source/kelgin/async.h"

namespace {
GIN_TEST("Async Immediate"){
	using namespace gin;

	EventLoop event_loop;
	WaitScope wait_scope{event_loop};

	Conveyor<size_t> number{5};

	Conveyor<bool> is_number = number.then([](size_t val){
		return val == 5;
	});

	wait_scope.poll();

	ErrorOr<bool> error_or_number = is_number.take();

	GIN_EXPECT(!error_or_number.isError(), error_or_number.error().message());
	GIN_EXPECT(error_or_number.isValue(), "Return is not a value");
	GIN_EXPECT(error_or_number.value(), "Value is not 5");
}

GIN_TEST("Async Adapt"){
	using namespace gin;

	EventLoop event_loop;
	WaitScope wait_scope{event_loop};

	auto feeder_conveyor = newConveyorAndFeeder<size_t>();

	feeder_conveyor.feeder->feed(5);

	ErrorOr<size_t> foo = feeder_conveyor.conveyor.take();

	GIN_EXPECT(!foo.isError(), foo.error().message());
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

	GIN_EXPECT(!foo.isError(), foo.error().message());
	GIN_EXPECT(foo.isValue(), "Return is not a value");
	GIN_EXPECT(foo.value() == 5, "Values not 5, but " + std::to_string(foo.value()));

	feeder_conveyor.feeder->feed(10);

	ErrorOr<size_t> bar = feeder_conveyor.conveyor.take();

	GIN_EXPECT(!foo.isError(), bar.error().message());
	GIN_EXPECT(bar.isValue(), "Return is not a value");
	GIN_EXPECT(bar.value() == 10, "Values not 10, but " + std::to_string(bar.value()));

	feeder_conveyor.feeder->feed(2);
	feeder_conveyor.feeder->feed(4234);

	ErrorOr<size_t> a = feeder_conveyor.conveyor.take();
	ErrorOr<size_t> b = feeder_conveyor.conveyor.take();

	GIN_EXPECT(!foo.isError(), a.error().message());
	GIN_EXPECT(a.isValue(), "Return is not a value");
	GIN_EXPECT(a.value() == 2, "Values not 2, but " + std::to_string(a.value()));

	GIN_EXPECT(!foo.isError(), b.error().message());
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

	GIN_EXPECT(!foo.isError(), foo.error().message());
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

	GIN_EXPECT(!foo.isError(), foo.error().message());
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

	ErrorOr<std::string> foo_10 = string_conveyor.take();

	GIN_EXPECT(!foo_10.isError(), foo_10.error().message());
	GIN_EXPECT(foo_10.isValue(), "Return is not a value");
	GIN_EXPECT(foo_10.value() == (std::string{"pre"} + std::to_string(11) + std::string{"post"}), "Values is not pre11post, but " + foo_10.value());

	ErrorOr<std::string> foo_20 = string_conveyor.take();

	GIN_EXPECT(!foo_20.isError(), foo_20.error().message());
	GIN_EXPECT(foo_20.isValue(), "Return is not a value");
	GIN_EXPECT(foo_20.value() == (std::string{"pre"} + std::to_string(22) + std::string{"post"}), "Values is not pre22post, but " + foo_20.value());

	ErrorOr<std::string> foo_30 = string_conveyor.take();

	GIN_EXPECT(!foo_30.isError(), foo_30.error().message());
	GIN_EXPECT(foo_30.isValue(), "Return is not a value");
	GIN_EXPECT(foo_30.value() == (std::string{"pre"} + std::to_string(33) + std::string{"post"}), "Values is not pre33post, but " + foo_30.value());
}

GIN_TEST("Async detach"){
	using namespace gin;

	EventLoop event_loop;
	WaitScope wait_scope{event_loop};

	int num = 0;

	Conveyor<int>{10}.then([&num](int bar){
		num = bar;
	}).detach();

	wait_scope.poll();

	GIN_EXPECT(num == 10, std::string{"Bad value: Expected 10, but got "} + std::to_string(num));
}

GIN_TEST("Async Merge"){
	using namespace gin;

	EventLoop event_loop;
	WaitScope wait_scope{event_loop};

	auto cam = Conveyor<int>{10}.merge();

	cam.second.attach(Conveyor<int>{11});

	cam.second.attach(Conveyor<int>{14});

	size_t elements_passed = 0;
	bool wrong_value = false;

	auto sink = cam.first.then([&elements_passed, &wrong_value](int foo){
		if(foo == 10 || foo == 11 || 14){
			++elements_passed;
		}else{
			wrong_value = true;
		}
	}).sink();

	wait_scope.poll();

	GIN_EXPECT(!wrong_value, std::string{"Expected values 10 or 11"});
	GIN_EXPECT(elements_passed == 3, std::string{"Expected 2 passed elements, got only "} + std::to_string(elements_passed));
}
}