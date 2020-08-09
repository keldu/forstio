#pragma once

#include <list>

#include "common.h"
#include "timer.h"

namespace gin {
class ConveyorNode {
public:
	virtual ~ConveyorNode() = default;
};

class ConveyorBase {
private:
	Own<ConveyorNode> node;

public:
	virtual ~ConveyorBase() = default;
};

template <typename Func, typename T> struct ReturnTypeHelper {
	typedef decltype(instance<Func>()(instance<T>())) Type;
};
template <typename Func> struct ReturnTypeHelper<Func, void> {
	typedef decltype(instance<Func>()()) Type;
};

template <typename Func, typename T>
using ReturnType = typename ReturnTypeHelper<Func, T>::Type;

template <typename T> class Conveyor;

template <typename T> Conveyor<T> chainedConveyorType(T *);

template <typename T> Conveyor<T> chainedConveyorType(Conveyor<T> *);

template <typename T>
using ChainedConveyors = decltype(chainedConveyorType((T *)nullptr));

template <typename Func, typename T>
using ConveyorResult = ChainedConveyors<ReturnType<Func, T>>;

template <typename T> class Conveyor : public ConveyorBase {
private:
public:
	template <typename Func, typename ErrorFunc>
	ConveyorResult<Func, T> then(Func &&func, ErrorFunc &&error_func);
};

class EventLoop;
class Event {
private:
	EventLoop &loop;
	Event **prev = nullptr;
	Event *next = nullptr;

public:
	Event(EventLoop &loop);
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
	Event *head = nullptr;
	Event **tail = &head;
	Event **next_insert_point = &head;
	Event **later_insert_point = &head;

	bool is_runnable = false;

	// functions
	void setRunnable(bool runnable);

	friend class WaitScope;
	void enterScope();
	void leaveScope();

public:
	EventLoop();
	~EventLoop();

	bool wait();
	bool wait(const std::chrono::steady_clock::duration &);
	bool wait(const std::chrono::steady_clock::time_point &);
	bool poll();
};

class WaitScope {
private:
	EventLoop &loop;

public:
	WaitScope(EventLoop &loop);
	~WaitScope();

	void wait();
	void wait(const std::chrono::steady_clock::duration &);
	void wait(const std::chrono::steady_clock::time_point &);
	void poll();
};

class InputConveyorNode : public ConveyorNode {
public:
};

template <typename T> class ConvertConveyorNode : public ConveyorNode {};

template <typename T, size_t S>
class ArrayBufferConveyorNode : public ConveyorNode, public Event {
private:
	std::array<T, S> storage;

public:
};
} // namespace gin
// Template inlining
namespace ent {}
