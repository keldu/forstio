#pragma once

#include <list>
#include <queue>

#include "common.h"
#include "error.h"
#include "timer.h"

namespace gin {
class ConveyorNode {
private:
	Own<ConveyorNode> child;
	ConveyorNode *parent = nullptr;

public:
	ConveyorNode();
	ConveyorNode(Own<ConveyorNode> &&child);
	virtual ~ConveyorNode() = default;

	void setParent(ConveyorNode *p);
};

class ConveyorBase {
private:
	Own<ConveyorNode> node;

public:
	ConveyorBase(Own<ConveyorNode> &&node);
	virtual ~ConveyorBase() = default;
};

class ConveyorStorage {
public:
	virtual ~ConveyorStorage() = default;

	virtual size_t space() const = 0;
	virtual size_t queued() const = 0;
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
	struct Helper {
	private:
		Error error;

	public:
		Helper(Error &&error);

		Error asError();
	};

	PropagateError::Helper operator()(const Error &error) const;
	PropagateError::Helper operator()(Error &&error);
};

template <typename T> class Conveyor : public ConveyorBase {
private:
public:
	template <typename Func, typename ErrorFunc = PropagateError>
	ConveyorResult<Func, T> then(Func &&func,
								 ErrorFunc &&error_func = PropagateError());
};

template <typename T> class ConveyorFeeder {
public:
	virtual ~ConveyorFeeder() = default;

	virtual void feed(T &&data) = 0;
	virtual void fail(Error &&error) = 0;

	virtual size_t space() const = 0;
	virtual size_t queued() const = 0;
};

template <typename T> struct ConveyorAndFeeder {
	Own<ConveyorFeeder<T>> feeder;
	Conveyor<T> conveyor;
};

template <typename T> ConveyorFeeder<T> newConveyorAndFeeder();

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

// Secret stuff
// Aka private semi hidden classes

template <typename T> class AdaptConveyorNode;

template <typename T> class AdaptConveyorFeeder : public ConveyorFeeder<T> {
private:
	AdaptConveyorNode<T> *feedee = nullptr;

	std::queue<T> storage;

public:
	~AdaptConveyorFeeder();

	void setFeedee(AdaptConveyorNode<T> *feedee);

	void feed(T &&value) override;
	void fail(Error &&error) override;

	size_t space() const override;
	size_t queued() const override;
};

template <typename T>
class AdaptConveyorNode : public ConveyorNode, public ConveyorStorage {
private:
	AdaptConveyorFeeder<T> *feeder = nullptr;

public:
	~AdaptConveyorNode();

	void setFeeder(AdaptConveyorFeeder<T> *feeder);

	size_t space() const override;
	size_t queued() const override;
};

template <typename T> class ConvertConveyorNode : public ConveyorNode {};

class QueueBufferConveyorNodeBase : public ConveyorNode,
									public ConveyorStorage {
public:
	virtual ~QueueBufferConveyorNodeBase() = default;
};

template <typename T, size_t S>
class QueueBufferConveyorNode : public QueueBufferConveyorNodeBase,
								public Event {
private:
	std::queue<T, std::array<T, S>> storage;

public:
	void fire() override;
};

template <typename T>
class QueueBufferConveyorNode<T, 0> : public QueueBufferConveyorNodeBase,
									  public Event {
private:
	std::queue<T> storage;

public:
	void fire() override;
};
} // namespace gin
// Template inlining
namespace gin {
template <typename T> AdaptConveyorFeeder<T>::~AdaptConveyorFeeder() {
	if (feedee) {
		feedee->setFeeder(nullptr);
		feedee = nullptr;
	}
}

template <typename T>
void AdaptConveyorFeeder<T>::setFeedee(AdaptConveyorNode<T> *feedee_p) {
	feedee = feedee_p;
}

template <typename T> AdaptConveyorNode<T>::~AdaptConveyorNode() {
	if (feeder) {
		feeder->setFeedee(nullptr);
		feeder = nullptr;
	}
}

template <typename T>
void AdaptConveyorNode<T>::setFeeder(AdaptConveyorFeeder<T> *feeder_p) {
	feeder = feeder_p;
}
} // namespace gin