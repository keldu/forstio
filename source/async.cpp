#include "async.h"

#include <algorithm>
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

void ConveyorStorage::setParent(ConveyorStorage *p) {
	/*
	 * parent check isn't needed, but is used
	 * for the assert, because the storage should
	 * be armed if there was an element present
	 * and a valid parent
	 */
	if (/*!parent && */ p && !isArmed() && queued() > 0) {
		assert(!parent);
		armNext();
	}
	parent = p;
}

ConveyorBase::ConveyorBase(Own<ConveyorNode> &&node_p,
						   ConveyorStorage *storage_p)
	: node{std::move(node_p)}, storage{storage_p} {}

Error PropagateError::operator()(const Error &error) const {
	Error err{error};
	return err;
}

Error PropagateError::operator()(Error &&error) { return std::move(error); }

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

SinkConveyor::SinkConveyor(Own<ConveyorNode> &&node_p)
	: node{std::move(node_p)} {}

void EventLoop::setRunnable(bool runnable) { is_runnable = runnable; }

EventLoop::EventLoop() {}

EventLoop::EventLoop(Own<EventPort> &&event_port)
	: event_port{std::move(event_port)} {}

EventLoop::~EventLoop() { assert(local_loop != this); }

void EventLoop::enterScope() {
	assert(!local_loop);
	local_loop = this;
}

void EventLoop::leaveScope() {
	assert(local_loop == this);
	local_loop = nullptr;
}

bool EventLoop::turnLoop(){
	size_t turn_step = 0;
	while (head && turn_step < 65536) {
		if (!turn()) {
			return false;
		}
		++turn_step;
	}
	return true;
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

	next_insert_point = &head;

	event->fire();

	return true;
}

bool EventLoop::wait(const std::chrono::steady_clock::duration &duration) {
	if (event_port) {
		event_port->wait(duration);
	}

	return turnLoop();
}

bool EventLoop::wait(const std::chrono::steady_clock::time_point &time_point) {
	if (event_port) {
		event_port->wait(time_point);
	}

	return turnLoop();
}

bool EventLoop::wait() {
	if (event_port) {
		event_port->wait();
	}

	return turnLoop();
}

bool EventLoop::poll() {
	if (event_port) {
		event_port->poll();
	}

	return turnLoop();
}

EventPort *EventLoop::eventPort() { return event_port.get(); }

ConveyorSinks &EventLoop::daemon() {
	if (!daemon_sink) {
		daemon_sink = heap<ConveyorSinks>();
	}
	return *daemon_sink;
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

void ConveyorSinks::destroySinkConveyorNode(ConveyorNode &node) {
	if (!isArmed()) {
		armLast();
	}

	delete_nodes.push(&node);
}

void ConveyorSinks::fail(Error &&error) {
	/// @todo call error_handler
}

void ConveyorSinks::add(Conveyor<void> &&sink) {
	auto nas = Conveyor<void>::fromConveyor(std::move(sink));
	Own<SinkConveyorNode> sink_node =
		heap<SinkConveyorNode>(std::move(nas.first), *this);
	if (nas.second) {
		nas.second->setParent(sink_node.get());
	}

	sink_nodes.push_back(std::move(sink_node));
}

void ConveyorSinks::fire() {
	while (!delete_nodes.empty()) {
		ConveyorNode *node = delete_nodes.front();
		/*auto erased = */ std::remove_if(sink_nodes.begin(), sink_nodes.end(),
										  [node](Own<ConveyorNode> &element) {
											  return node == element.get();
										  });
		delete_nodes.pop();
	}
}

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

void detachConveyor(Conveyor<void> &&conveyor) {
	EventLoop &loop = currentEventLoop();
	ConveyorSinks &sink = loop.daemon();
	sink.add(std::move(conveyor));
}
} // namespace gin
