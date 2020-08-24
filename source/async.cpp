#include "async.h"

#include <cassert>

namespace gin {
namespace {
thread_local EventLoop *local_loop = nullptr;

EventLoop &currentEventLoop() {
	EventLoop *loop = local_loop;
	assert(loop);
	return *loop;
}
} // namespace

ConveyorNode::ConveyorNode() : child{nullptr} {}

ConveyorNode::ConveyorNode(Own<ConveyorNode> &&node) : child{std::move(node)} {}

void ConveyorStorage::setParent(ConveyorStorage *p) { parent = p; }

ConveyorBase::ConveyorBase(Own<ConveyorNode> &&node_p,
						   ConveyorStorage *storage_p)
	: node{std::move(node_p)}, storage{storage_p} {}

PropagateError::Helper::Helper(Error &&error) : error{std::move(error)} {}

Error PropagateError::Helper::asError() { return std::move(error); }

PropagateError::Helper PropagateError::operator()(const Error &error) const {
	Error err{error};
	return PropagateError::Helper{std::move(err)};
}

PropagateError::Helper PropagateError::operator()(Error &&error) {
	return PropagateError::Helper{std::move(error)};
}

Event::Event() : Event(currentEventLoop()) {}

Event::Event(EventLoop &loop) : loop{loop} {}

Event::~Event() { disarm(); }

void Event::armNext() {
	assert(&loop == local_loop);
	if (prev == nullptr) {
		// Push the next_insert_point back by one
		// and inserts itself before that
		next = *loop.next_insert_point;
		prev = loop.next_insert_point;
		*prev = this;
		if (next) {
			next->prev = &next;
		}

		// Set the new insertion ptr location to next
		loop.next_insert_point = &next;

		// Pushes back the later insert point if it was pointing at the
		// previous event
		if (loop.later_insert_point == prev) {
			loop.later_insert_point = &next;
		}

		// If tail points at the same location then
		// we are at the end and have to update tail then.
		// Technically should be possible by checking if
		// next is a `nullptr`
		if (loop.tail == prev) {
			loop.tail = &next;
		}

		loop.setRunnable(true);
	}
}

void Event::armLater() {
	assert(&loop == local_loop);

	if (prev == nullptr) {
		next = *loop.later_insert_point;
		prev = loop.later_insert_point;
		*prev = this;
		if (next) {
			next->prev = &next;
		}

		loop.later_insert_point = &next;
		if (loop.tail == prev) {
			loop.tail = &next;
		}

		loop.setRunnable(true);
	}
}

void Event::armLast() {
	assert(&loop == local_loop);

	if (prev == nullptr) {
		next = *loop.later_insert_point;
		prev = loop.later_insert_point;
		*prev = this;
		if (next) {
			next->prev = &next;
		}

		if (loop.tail == prev) {
			loop.tail = &next;
		}

		loop.setRunnable(true);
	}
}

void Event::disarm() {
	if (prev != nullptr) {
		if (loop.tail == &next) {
			loop.tail = prev;
		}

		if (loop.next_insert_point == &next) {
			loop.next_insert_point = prev;
		}

		*prev = next;
		if (next) {
			next->prev = prev;
		}

		prev = nullptr;
		next = nullptr;
	}
}

bool Event::isArmed() const { return prev != nullptr; }

void EventLoop::setRunnable(bool runnable) { is_runnable = runnable; }

EventLoop::EventLoop() {}

EventLoop::~EventLoop() { assert(local_loop != this); }

void EventLoop::enterScope() {
	assert(!local_loop);
	local_loop = this;
}

void EventLoop::leaveScope() {
	assert(local_loop == this);
	local_loop = nullptr;
}

bool EventLoop::turn() {
	Event *event = head;

	if (!event) {
		return false;
	}

	head = event->next;
	if (head) {
		head->prev = &head;
	}

	next_insert_point = &head;
	if (later_insert_point == &event->next) {
		later_insert_point = &head;
	}
	if (tail == &event->next) {
		tail = &head;
	}

	event->next = nullptr;
	event->prev = nullptr;

	event->fire();

	next_insert_point = &head;

	return true;
}

bool EventLoop::wait(const std::chrono::steady_clock::duration &duration) {
	return false;
}

bool EventLoop::wait(const std::chrono::steady_clock::time_point &time_point) {
	return false;
}

bool EventLoop::wait() { return false; }

bool EventLoop::poll() {
	while (head) {
		if (!turn()) {
			return false;
		}
	}
	return true;
}

WaitScope::WaitScope(EventLoop &loop) : loop{loop} { loop.enterScope(); }

WaitScope::~WaitScope() { loop.leaveScope(); }

void WaitScope::wait() { loop.wait(); }

void WaitScope::wait(const std::chrono::steady_clock::duration &duration) {
	loop.wait(duration);
}

void WaitScope::wait(const std::chrono::steady_clock::time_point &time_point) {
	loop.wait(time_point);
}

void WaitScope::poll() { loop.poll(); }

ConvertConveyorNodeBase::ConvertConveyorNodeBase(Own<ConveyorNode> &&dep)
	: ConveyorNode{std::move(dep)} {}

void ConvertConveyorNodeBase::getResult(ErrorOrValue &err_or_val) {
	getImpl(err_or_val);
}

void AttachConveyorNodeBase::getResult(ErrorOrValue &err_or_val) {
	if (child) {
		child->getResult(err_or_val);
	}
}
} // namespace gin