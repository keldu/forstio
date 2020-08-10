#pragma once

#include <list>

#include "common.h"
#include "timer.h"

namespace gin {
class ConveyorNode {
private:
	Own<ConveyorNode> node;
	ConveyorNode* parent = nullptr;
public:
	ConveyorNode();
	ConveyorNode(Own<ConveyorNode>&& node);
	virtual ~ConveyorNode() = default;

	void setParent(ConveyorNode* p);
};

class ConveyorBase {
private:
	Own<ConveyorNode> node;
public:
	ConveyorBase(Own<ConveyorNode>&& node);
	virtual ~ConveyorBase() = default;
};

template <typename T> class Conveyor;

template <typename T> Conveyor<T> chainedConveyorType(T *);

template <typename T> Conveyor<T> chainedConveyorType(Conveyor<T> *);

template <typename T>
using ChainedConveyors = decltype(chainedConveyorType((T *)nullptr));

template <typename Func, typename T>
using ConveyorResult = ChainedConveyors<ReturnType<Func, T>>;

struct PropagateError {
public:
	struct PropagateErrorHelper {
		Error error;

		PropagateError asError();
	};

	PropagateErrorHelper operator(const Error& error) const;
	PropagateErrorHelper operator(Error&& error);
};

template <typename T> class Conveyor : public ConveyorBase {
private:
public:
	template <typename Func, typename ErrorFunc>
	ConveyorResult<Func, T> then(Func &&func, ErrorFunc &&error_func);
};

template<typename T>
class ConveyorFeeder {
private:
	Conveyor<T>* entry = nullptr;

	friend class Conveyor<T>;

	void entryDestroyed();
public:
	ConveyorFeeder(Conveyor<T>& conv);
	void feed(T&& data);
};

template<typename T>
struct ConveyorAndFeeder {
	Own<ConveyorFeeder<T>> feeder;
	Conveyor<T> conveyor;
};

template<typename T>
ConveyorFeeder<T> newConveyorAndFeeder();

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

template <typename T> class ConvertConveyorNode : public ConveyorNode {};

template <typename T, size_t S>
class QueueBufferConveyorNode : public ConveyorNode, public Event {
private:
	std::queue<T, std::array<T,S>> storage;
public:
	void fire() override;
};

template<typename T>
class QueueBufferConveyorNode<T> : public ConveyorNode, public Event {
private:
	std::queue<T> storage;
public:
	void fire() override;
};
} // namespace gin
// Template inlining
namespace ent {}
