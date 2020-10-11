#pragma once

#include <functional>
#include <limits>
#include <list>
#include <queue>

#include "common.h"
#include "error.h"
#include "timer.h"

namespace gin {
class ConveyorNode {
protected:
	Own<ConveyorNode> child;

public:
	ConveyorNode();
	ConveyorNode(Own<ConveyorNode> &&child);
	virtual ~ConveyorNode() = default;

	virtual void getResult(ErrorOrValue &err_or_val) = 0;
};

class EventLoop;
class Event {
private:
	EventLoop &loop;
	Event **prev = nullptr;
	Event *next = nullptr;

	friend class EventLoop;

public:
	Event();
	Event(EventLoop &loop);
	virtual ~Event();

	virtual void fire() = 0;

	void armNext();
	void armLater();
	void armLast();
	void disarm();

	bool isArmed() const;
};

class ConveyorStorage : public Event {
protected:
	ConveyorStorage *parent = nullptr;

public:
	virtual ~ConveyorStorage() = default;

	virtual size_t space() const = 0;
	virtual size_t queued() const = 0;
	virtual void childFired() = 0;

	void setParent(ConveyorStorage *parent);
};

class ConveyorBase {
protected:
	Own<ConveyorNode> node;

	ConveyorStorage *storage;

public:
	ConveyorBase(Own<ConveyorNode> &&node_p,
				 ConveyorStorage *storage_p = nullptr);
	virtual ~ConveyorBase() = default;

	ConveyorBase(ConveyorBase &&) = default;
	ConveyorBase &operator=(ConveyorBase &&) = default;

	void get(ErrorOrValue &err_or_val);
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
	Error operator()(const Error &error) const;
	Error operator()(Error &&error);
};

template <typename T> class Conveyor : public ConveyorBase {
public:
	/*
	 * Construct a immediately fulfilled node
	 */
	Conveyor(FixVoid<T> value);
	/*
	 * empty promise
	 * @todo remove this
	 */
	Conveyor(Own<ConveyorNode> &&node_p, ConveyorStorage *storage_p);

	Conveyor(Conveyor<T> &&) = default;
	Conveyor<T> &operator=(Conveyor<T> &&) = default;

	/*
	 * This method converts passed values or errors from children
	 */
	template <typename Func, typename ErrorFunc = PropagateError>
	ConveyorResult<Func, T> then(Func &&func,
								 ErrorFunc &&error_func = PropagateError());

	/*
	 * This method adds a buffer node in the conveyor chains and acts as a
	 * scheduler interruption point.
	 */
	Conveyor<T> buffer(size_t limit = std::numeric_limits<size_t>::max());

	/*
	 * This method just takes ownership of any supplied types
	 */
	template <typename... Args> Conveyor<T> attach(Args &&... args);

	/*
	 *
	 */
	Conveyor<T> limit(size_t val = std::numeric_limits<size_t>::max());

	/*
	 *
	 */
	template <typename ErrorFunc> void detach(ErrorFunc &&err_func);

	// Waiting and resolving
	ErrorOr<FixVoid<T>> take();

	void poll();

	// helper
	static Conveyor<T> toConveyor(Own<ConveyorNode> &&node,
								  ConveyorStorage *is_storage = nullptr);

	// helper
	static std::pair<Own<ConveyorNode>, ConveyorStorage *>
	fromConveyor(Conveyor<T> &&conveyor);
};

template <typename T> class ConveyorFeeder {
public:
	virtual ~ConveyorFeeder() = default;

	virtual void feed(T &&data) = 0;
	virtual void fail(Error &&error) = 0;

	virtual size_t space() const = 0;
	virtual size_t queued() const = 0;
};

template <> class ConveyorFeeder<void> {
public:
	virtual ~ConveyorFeeder() = default;

	virtual void feed(Void &&value = Void{}) = 0;
	virtual void fail(Error &&error) = 0;

	virtual size_t space() const = 0;
	virtual size_t queued() const = 0;
};

template <typename T> struct ConveyorAndFeeder {
	Own<ConveyorFeeder<T>> feeder;
	Conveyor<T> conveyor;
};

template <typename T> ConveyorAndFeeder<T> newConveyorAndFeeder();

template <typename T> ConveyorAndFeeder<T> oneTimeConveyorAndFeeder();

enum class Signal : uint8_t { Terminate, User1 };

class EventPort {
public:
	virtual ~EventPort() = default;

	virtual Conveyor<void> onSignal(Signal signal) = 0;

	virtual void poll() = 0;
	virtual void wait() = 0;
	virtual void wait(const std::chrono::steady_clock::duration &) = 0;
	virtual void wait(const std::chrono::steady_clock::time_point &) = 0;
};

class SinkConveyorNode;

class ConveyorSink : public Event {
private:
	friend class SinkConveyorNode;

	void destroySinkConveyorNode(ConveyorNode &sink_node);
	void fail(Error &&error);

	std::list<Own<ConveyorNode>> sink_nodes;

	std::queue<ConveyorNode *> delete_nodes;

	std::function<void(Error &&error)> error_handler;

public:
	ConveyorSink() = default;

	void add(Conveyor<void> &&node);

	void fire() override;
};

class EventLoop {
private:
	friend class Event;
	Event *head = nullptr;
	Event **tail = &head;
	Event **next_insert_point = &head;
	Event **later_insert_point = &head;

	bool is_runnable = false;

	Own<EventPort> event_port = nullptr;

	Own<ConveyorSink> daemon_sink = nullptr;

	// functions
	void setRunnable(bool runnable);

	friend class WaitScope;
	void enterScope();
	void leaveScope();

	bool turn();

public:
	EventLoop();
	EventLoop(Own<EventPort> &&port);
	~EventLoop();

	bool wait();
	bool wait(const std::chrono::steady_clock::duration &);
	bool wait(const std::chrono::steady_clock::time_point &);
	bool poll();

	EventPort *eventPort();

	ConveyorSink &daemon();
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

template <typename Func> ConveyorResult<Func, void> yieldNext(Func &&func);

template <typename Func> ConveyorResult<Func, void> yieldLater(Func &&func);

template <typename Func> ConveyorResult<Func, void> yieldLast(Func &&func);
} // namespace gin

// Secret stuff
// Aka private semi hidden classes
namespace gin {

template <typename Out, typename In> struct FixVoidCaller {
	template <typename Func> static Out apply(Func &func, In &&in) {
		return func(std::move(in));
	}
};

template <typename Out> struct FixVoidCaller<Out, Void> {
	template <typename Func> static Out apply(Func &func, Void &&in) {
		(void)in;
		return func();
	}
};

template <typename In> struct FixVoidCaller<Void, In> {
	template <typename Func> static Void apply(Func &func, In &&in) {
		func(std::move(in));
		return Void{};
	}
};

template <> struct FixVoidCaller<Void, Void> {
	template <typename Func> static Void apply(Func &func, Void &&in) {
		(void)in;
		func();
		return Void{};
	}
};

template <typename T> class AdaptConveyorNode;

template <typename T>
class AdaptConveyorFeeder : public ConveyorFeeder<UnfixVoid<T>> {
private:
	AdaptConveyorNode<T> *feedee = nullptr;

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

	std::queue<ErrorOr<T>> storage;

public:
	~AdaptConveyorNode();

	void setFeeder(AdaptConveyorFeeder<T> *feeder);

	void feed(T &&value);
	void fail(Error &&error);

	// ConveyorNode
	void getResult(ErrorOrValue &err_or_val) override;

	// ConveyorStorage
	size_t space() const override;
	size_t queued() const override;

	void childFired() override {}

	// Event
	void fire() override;
};

template <typename T> class OneTimeConveyorNode;

template <typename T>
class OneTimeConveyorFeeder : public ConveyorFeeder<UnfixVoid<T>> {
private:
	OneTimeConveyorNode<T> *feedee = nullptr;

public:
	~OneTimeConveyorFeeder();

	void setFeedee(OneTimeConveyorNode<T> *feedee);

	void feed(T &&value) override;
	void fail(Error &&error) override;

	size_t space() const override;
	size_t queued() const override;
};

template <typename T>
class OneTimeConveyorNode : public ConveyorNode, public ConveyorStorage {
private:
	OneTimeConveyorFeeder<T> *feeder = nullptr;

	bool passed = false;
	Maybe<ErrorOr<T>> storage = std::nullopt;

public:
	~OneTimeConveyorNode();

	void setFeeder(OneTimeConveyorFeeder<T> *feeder);

	void feed(T &&value);
	void fail(Error &&error);

	// ConveyorNode
	void getResult(ErrorOrValue &err_or_val) override;

	// ConveyorStorage
	size_t space() const override;
	size_t queued() const override;

	void childFired() override {}

	// Event
	void fire() override;
};

class QueueBufferConveyorNodeBase : public ConveyorNode,
									public ConveyorStorage {
public:
	QueueBufferConveyorNodeBase(Own<ConveyorNode> &&dep)
		: ConveyorNode(std::move(dep)) {}
	virtual ~QueueBufferConveyorNodeBase() = default;
};

template <typename T>
class QueueBufferConveyorNode : public QueueBufferConveyorNodeBase {
private:
	std::queue<ErrorOr<T>> storage;
	size_t max_store;

public:
	QueueBufferConveyorNode(Own<ConveyorNode> &&dep, size_t max_size)
		: QueueBufferConveyorNodeBase(std::move(dep)), max_store{max_size} {}
	// Event
	void fire() override {
		if (child) {
			if (!storage.empty()) {
				if (storage.front().isError()) {
					if (storage.front().error().isCritical()) {
						child = nullptr;
					}
				}
			}
		}
		if (parent) {
			parent->childFired();
			if (!storage.empty()) {
				armLater();
			}
		}
	}
	// ConveyorNode
	void getResult(ErrorOrValue &eov) override {
		ErrorOr<T> &err_or_val = eov.as<T>();
		err_or_val = std::move(storage.front());
		storage.pop();
	}

	// ConveyorStorage
	size_t space() const override { return max_store - storage.size(); }
	size_t queued() const override { return storage.size(); }

	void childFired() override {
		if (child && storage.size() < max_store) {
			ErrorOr<T> eov;
			child->getResult(eov);
			storage.push(std::move(eov));
			if (!isArmed()) {
				armLater();
			}
		}
	}
};

class AttachConveyorNodeBase : public ConveyorNode {
public:
	AttachConveyorNodeBase(Own<ConveyorNode> &&dep)
		: ConveyorNode(std::move(dep)) {}

	void getResult(ErrorOrValue &err_or_val) override;
};

template <typename... Args>
class AttachConveyorNode : public AttachConveyorNodeBase {
private:
	std::tuple<Args...> attached_data;

public:
	AttachConveyorNode(Own<ConveyorNode> &&dep, Args &&... args)
		: AttachConveyorNodeBase(std::move(dep)), attached_data{
													  std::move(args...)} {}
};

class ConvertConveyorNodeBase : public ConveyorNode {
public:
	ConvertConveyorNodeBase(Own<ConveyorNode> &&dep);

	void getResult(ErrorOrValue &err_or_val) override;

	virtual void getImpl(ErrorOrValue &err_or_val) = 0;
};

template <typename T, typename DepT, typename Func, typename ErrorFunc>
class ConvertConveyorNode : public ConvertConveyorNodeBase {
private:
	Func func;
	ErrorFunc error_func;

public:
	ConvertConveyorNode(Own<ConveyorNode> &&dep, Func &&func,
						ErrorFunc &&error_func)
		: ConvertConveyorNodeBase(std::move(dep)), func{std::move(func)},
		  error_func{std::move(error_func)} {}

	void getImpl(ErrorOrValue &err_or_val) override {
		ErrorOr<DepT> dep_eov;
		ErrorOr<T> &eov = err_or_val.as<T>();
		if (child) {
			child->getResult(dep_eov);
			if (dep_eov.isValue()) {
				eov = FixVoidCaller<T, DepT>::apply(func,
													std::move(dep_eov.value()));
			} else if (dep_eov.isError()) {
				eov = error_func(std::move(dep_eov.error()));
			} else {
				eov = criticalError("No value set in dependency");
			}
		} else {
			eov = criticalError("Conveyor doesn't have child");
		}
	}
};

class SinkConveyorNode : public ConveyorNode, public ConveyorStorage {
private:
	ConveyorSink *conveyor_sink;

public:
	SinkConveyorNode(Own<ConveyorNode> &&node, ConveyorSink &conv_sink)
		: ConveyorNode(std::move(node)), conveyor_sink{&conv_sink} {}

	// Event only queued if a critical error occured
	void fire() override {
		// Queued for destruction of children, because this acts as a sink and
		// no other event should be here
		child = nullptr;

		if (conveyor_sink) {
			conveyor_sink->destroySinkConveyorNode(*this);
			conveyor_sink = nullptr;
		}
	}

	// ConveyorStorage
	size_t space() const override { return 1; }
	size_t queued() const override { return 0; }

	// ConveyorNode
	void getResult(ErrorOrValue &err_or_val) override {
		err_or_val.as<Void>() =
			criticalError("In a sink node no result can be returned");
	}

	// ConveyorStorage
	void childFired() override {
		if (child) {
			ErrorOr<Void> dep_eov;
			child->getResult(dep_eov);
			if (dep_eov.isError()) {
				if (dep_eov.error().isCritical()) {
					if (!isArmed()) {
						armLast();
					}
				}
				if (conveyor_sink) {
					conveyor_sink->fail(std::move(dep_eov.error()));
				}
			}
		}
	}
};

class ImmediateConveyorNodeBase : public ConveyorNode, public ConveyorStorage {
private:
public:
};

template <typename T>
class ImmediateConveyorNode : public ImmediateConveyorNodeBase {
private:
	FixVoid<T> value;
	bool retrieved;

public:
	ImmediateConveyorNode(FixVoid<T> &&val);

	// ConveyorStorage
	size_t space() const override;
	size_t queued() const override;

	void childFired() override;

	// ConveyorNode
	void getResult(ErrorOrValue &err_or_val) override {
		if (retrieved) {
			err_or_val.as<FixVoid<T>>() = criticalError("Already taken value");
		} else {
			err_or_val.as<FixVoid<T>>() = std::move(value);
			retrieved = true;
		}
	}

	// Event
	void fire() override;
};

template <typename T>
ImmediateConveyorNode<T>::ImmediateConveyorNode(FixVoid<T> &&val)
	: value{std::move(val)}, retrieved{false} {}

template <typename T> size_t ImmediateConveyorNode<T>::space() const {
	return 0;
}

template <typename T> size_t ImmediateConveyorNode<T>::queued() const {
	return retrieved ? 0 : 1;
}

template <typename T> void ImmediateConveyorNode<T>::childFired() {
	// Impossible
}

template <typename T> void ImmediateConveyorNode<T>::fire() {
	if (parent) {
		parent->childFired();
	}
}

} // namespace gin
#include <cassert>
// Template inlining
namespace gin {
template <typename T> T reduceErrorOrType(T *);

template <typename T> T reduceErrorOrType(ErrorOr<T> *);

template <typename T>
using ReduceErrorOr = decltype(reduceErrorOrType((T *)nullptr));

template <typename T>
Conveyor<T>::Conveyor(FixVoid<T> value) : ConveyorBase(nullptr, nullptr) {
	// Is there any way to do  this?
	// @todo new ConveyorBase constructor for Immediate values
	auto immediate = heap<ImmediateConveyorNode<FixVoid<T>>>(std::move(value));
	storage = reinterpret_cast<ConveyorStorage *>(immediate.get());
	node = std::move(immediate);
}

template <typename T>
Conveyor<T>::Conveyor(Own<ConveyorNode> &&node_p, ConveyorStorage *storage_p)
	: ConveyorBase(std::move(node_p), storage_p) {}

template <typename T>
template <typename Func, typename ErrorFunc>
ConveyorResult<Func, T> Conveyor<T>::then(Func &&func, ErrorFunc &&error_func) {
	Own<ConveyorNode> conversion_node =
		heap<ConvertConveyorNode<FixVoid<ReduceErrorOr<ReturnType<Func, T>>>,
								 FixVoid<T>, Func, ErrorFunc>>(
			std::move(node), std::move(func), std::move(error_func));

	return Conveyor<ReduceErrorOr<ReturnType<Func, T>>>::toConveyor(
		std::move(conversion_node), storage);
}

template <typename T> Conveyor<T> Conveyor<T>::buffer(size_t size) {
	Own<QueueBufferConveyorNode<FixVoid<T>>> storage_node =
		heap<QueueBufferConveyorNode<FixVoid<T>>>(std::move(node), size);
	ConveyorStorage *storage_ptr =
		static_cast<ConveyorStorage *>(storage_node.get());
	storage->setParent(storage_ptr);
	return Conveyor<T>{std::move(storage_node), storage_ptr};
}

template <typename T>
template <typename... Args>
Conveyor<T> Conveyor<T>::attach(Args &&... args) {
	Own<AttachConveyorNode<Args...>> attach_node =
		heap<AttachConveyorNode<Args...>>(std::move(node), std::move(args...));
	return Conveyor<T>{std::move(attach_node), storage};
}

void detachConveyor(Conveyor<void> &&conveyor);

template <typename T>
template <typename ErrorFunc>
void Conveyor<T>::detach(ErrorFunc &&func) {
	detachConveyor(std::move(then([](T &&) {}, std::move(func))));
}

template <>
template <typename ErrorFunc>
void Conveyor<void>::detach(ErrorFunc &&func) {
	detachConveyor(std::move(then([]() {}, std::move(func))));
}

template <typename T>
Conveyor<T> Conveyor<T>::toConveyor(Own<ConveyorNode> &&node,
									ConveyorStorage *storage) {
	return Conveyor<T>{std::move(node), storage};
}

template <typename T>
std::pair<Own<ConveyorNode>, ConveyorStorage *>
Conveyor<T>::fromConveyor(Conveyor<T> &&conveyor) {
	return std::make_pair(std::move(conveyor.node), conveyor.storage);
}

template <typename T> ErrorOr<FixVoid<T>> Conveyor<T>::take() {
	if (storage) {
		if (storage->queued() > 0) {
			ErrorOr<FixVoid<T>> result;
			node->getResult(result);
			return ErrorOr<FixVoid<T>>{result};
		} else {
			return ErrorOr<FixVoid<T>>{
				recoverableError("Conveyor buffer has no elements")};
		}
	} else {
		return ErrorOr<FixVoid<T>>{criticalError("Conveyor in invalid state")};
	}
}

template <typename T> ConveyorAndFeeder<T> newConveyorAndFeeder() {
	Own<AdaptConveyorFeeder<FixVoid<T>>> feeder =
		heap<AdaptConveyorFeeder<FixVoid<T>>>();
	Own<AdaptConveyorNode<FixVoid<T>>> node =
		heap<AdaptConveyorNode<FixVoid<T>>>();

	feeder->setFeedee(node.get());
	node->setFeeder(feeder.get());

	ConveyorStorage *storage_ptr = static_cast<ConveyorStorage *>(node.get());

	return ConveyorAndFeeder<T>{
		std::move(feeder),
		Conveyor<T>::toConveyor(std::move(node), storage_ptr)};
}

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

template <typename T> void AdaptConveyorFeeder<T>::feed(T &&value) {
	if (feedee) {
		feedee->feed(std::move(value));
	}
}

template <typename T> void AdaptConveyorFeeder<T>::fail(Error &&error) {
	if (feedee) {
		feedee->fail(std::move(error));
	}
}

template <typename T> size_t AdaptConveyorFeeder<T>::queued() const {
	if (feedee) {
		return feedee->queued();
	}
	return 0;
}

template <typename T> size_t AdaptConveyorFeeder<T>::space() const {
	if (feedee) {
		return feedee->space();
	}
	return 0;
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

template <typename T> void AdaptConveyorNode<T>::feed(T &&value) {
	storage.push(std::move(value));
	armNext();
}

template <typename T> void AdaptConveyorNode<T>::fail(Error &&error) {
	storage.push(std::move(error));
	armNext();
}

template <typename T> size_t AdaptConveyorNode<T>::queued() const {
	return storage.size();
}

template <typename T> size_t AdaptConveyorNode<T>::space() const {
	return std::numeric_limits<size_t>::max() - storage.size();
}

template <typename T>
void AdaptConveyorNode<T>::getResult(ErrorOrValue &err_or_val) {
	if (!storage.empty()) {
		err_or_val.as<T>() = std::move(storage.front());
		storage.pop();
	} else {
		err_or_val.as<T>() =
			criticalError("Signal for retrieval of storage sent even though no "
						  "data is present");
	}
}

template <typename T> void AdaptConveyorNode<T>::fire() {
	if (parent) {
		parent->childFired();

		if (storage.size() > 0) {
			armLater();
		}
	}
}

template <typename T> OneTimeConveyorFeeder<T>::~OneTimeConveyorFeeder() {
	if (feedee) {
		feedee->setFeeder(nullptr);
		feedee = nullptr;
	}
}

template <typename T>
void OneTimeConveyorFeeder<T>::setFeedee(OneTimeConveyorNode<T> *feedee_p) {
	feedee = feedee_p;
}

template <typename T> void OneTimeConveyorFeeder<T>::feed(T &&value) {
	if (feedee) {
		feedee->feed(std::move(value));
	}
}

template <typename T> void OneTimeConveyorFeeder<T>::fail(Error &&error) {
	if (feedee) {
		feedee->fail(std::move(error));
	}
}

template <typename T> size_t OneTimeConveyorFeeder<T>::queued() const {
	if (feedee) {
		return feedee->queued();
	}
	return 0;
}

template <typename T> size_t OneTimeConveyorFeeder<T>::space() const {
	if (feedee) {
		return feedee->space();
	}
	return 0;
}

template <typename T> OneTimeConveyorNode<T>::~OneTimeConveyorNode() {
	if (feeder) {
		feeder->setFeedee(nullptr);
		feeder = nullptr;
	}
}

template <typename T>
void OneTimeConveyorNode<T>::setFeeder(OneTimeConveyorFeeder<T> *feeder_p) {
	feeder = feeder_p;
}

template <typename T> void OneTimeConveyorNode<T>::feed(T &&value) {
	storage = std::move(value);
	armNext();
}

template <typename T> void OneTimeConveyorNode<T>::fail(Error &&error) {
	storage = std::move(error);
	armNext();
}

template <typename T> size_t OneTimeConveyorNode<T>::queued() const {
	return storage.has_value() ? 1 : 0;
}

template <typename T> size_t OneTimeConveyorNode<T>::space() const {
	return passed ? 0 : 1;
}

template <typename T>
void OneTimeConveyorNode<T>::getResult(ErrorOrValue &err_or_val) {
	if (storage.has_value()) {
		err_or_val.as<T>() = std::move(storage.value());
		storage = std::nullopt;
	} else {
		err_or_val.as<T>() =
			criticalError("Signal for retrieval of storage sent even though no "
						  "data is present");
	}
}

template <typename T> void OneTimeConveyorNode<T>::fire() {
	if (parent) {
		parent->childFired();
	}
}

} // namespace gin
