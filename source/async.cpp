#include "async.h"

#include <cassert>

namespace gin {
namespace {
thread_local EventLoop *local_loop = nullptr;
}

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
	if (!prev) {
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

bool EventLoop::wait(const std::chrono::steady_clock::duration &duration) {
	return false;
}

bool EventLoop::wait(const std::chrono::steady_clock::time_point &time_point) {
	return false;
}

bool EventLoop::wait() { return false; }

bool EventLoop::poll() { return false; }

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
} // namespace gin