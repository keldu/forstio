#pragma once

#include "time.h"
#include "common.h"

#include <list>

namespace gin {
class ConveyorBase {
public:
	virtual ~ConveyorBase() = default;
};
class EventLoop;
class Event {
private:
	EventLoop& loop;
	Event* prev = nullptr;
	Event** next = nullptr;
public:
	Event(EventLoop& loop);
	virtual ~Event();

	virtual void fire() = 0;

	void armNext();
	void armLater();
	void armLast();
	void disarm();
};

class EventLoop {
private:
	friend class Event;
	Event* head = nullptr;
	Event** tail = &head;
	Event** next_insert_point = &head;
	Event** later_insert_point = &head;

	bool is_runnable = false;

	// functions
	void setRunnable(bool runnable);

	friend class WaitScope;
	void enterScope();
	void leaveScope();

	bool wait();
	bool wait(const std::chrono::steady_clock::duration&);
	bool poll();

	class Impl;
	Own<Impl> impl;
public:
	EventLoop();
	~EventLoop();
};

class WaitScope {
private:
	EventLoop& loop;
public:
	WaitScope(EventLoop& loop);
	~WaitScope();

	void wait();
	void wait(const std::chrono::steady_clock::duration&);
	void wait(const std::chrono::steady_clock::timepoint&);
	void poll();
};
}