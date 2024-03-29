#pragma once

#include "common.h"
#include "error.h"
#include "timer.h"

#include <functional>
#include <limits>
#include <list>
#include <queue>
#include <type_traits>

namespace saw {
class ConveyorNode {
public:
	ConveyorNode();
	virtual ~ConveyorNode() = default;

	virtual void getResult(ErrorOrValue &err_or_val) = 0;
};

class EventLoop;
/*
 * Event class similar to capn'proto.
 * https://github.com/capnproto/capnproto
 */
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

class ConveyorStorage {
protected:
	ConveyorStorage *parent = nullptr;
	ConveyorStorage *child_storage = nullptr;

public:
	ConveyorStorage(ConveyorStorage *child);
	virtual ~ConveyorStorage();

	virtual size_t space() const = 0;
	virtual size_t queued() const = 0;
	virtual void childHasFired() = 0;
	virtual void parentHasFired() = 0;

	virtual void setParent(ConveyorStorage *parent) = 0;
	void unlinkChild();
};

class ConveyorEventStorage : public ConveyorStorage, public Event {
public:
	ConveyorEventStorage(ConveyorStorage *child);
	virtual ~ConveyorEventStorage() = default;

	void setParent(ConveyorStorage *parent) override;
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

// template <typename T> Conveyor<T> chainedConveyorType(Conveyor<T> *);

template <typename T> T removeErrorOrType(T *);

template <typename T> T removeErrorOrType(ErrorOr<T> *);

template <typename T>
using RemoveErrorOr = decltype(removeErrorOrType((T *)nullptr));

template <typename T>
using ChainedConveyors = decltype(chainedConveyorType((T *)nullptr));

template <typename Func, typename T>
using ConveyorResult = ChainedConveyors<RemoveErrorOr<ReturnType<Func, T>>>;

struct PropagateError {
public:
	Error operator()(const Error &error) const;
	Error operator()(Error &&error);
};

class SinkConveyor {
private:
	Own<ConveyorNode> node;

public:
	SinkConveyor();
	SinkConveyor(Own<ConveyorNode> &&node);

	SinkConveyor(SinkConveyor &&) = default;
	SinkConveyor &operator=(SinkConveyor &&) = default;
};

template <typename T> class MergeConveyorNodeData;

template <typename T> class MergeConveyor {
private:
	Lent<MergeConveyorNodeData<T>> data;

public:
	MergeConveyor(Lent<MergeConveyorNodeData<T>> d);
	~MergeConveyor();

	void attach(Conveyor<T> conveyor);
};

/**
 * Main interface for async operations.
 */
template <typename T> class Conveyor final : public ConveyorBase {
public:
	/**
	 * Construct an immediately fulfilled node
	 */
	Conveyor(FixVoid<T> value);

	/**
	 * Construct an immediately failed node
	 */
	Conveyor(Error &&error);

	/**
	 * Construct a conveyor with a child node and the next storage point
	 */
	Conveyor(Own<ConveyorNode> node_p, ConveyorStorage *storage_p);

	Conveyor(Conveyor<T> &&) = default;
	Conveyor<T> &operator=(Conveyor<T> &&) = default;

	/**
	 * This method converts values or errors from children
	 */
	template <typename Func, typename ErrorFunc = PropagateError>
	[[nodiscard]] ConveyorResult<Func, T>
	then(Func &&func, ErrorFunc &&error_func = PropagateError());

	/**
	 * This method adds a buffer node in the conveyor chains which acts as a
	 * scheduler interrupt point and collects elements up to the supplied limit.
	 */
	[[nodiscard]] Conveyor<T>
	buffer(size_t limit = std::numeric_limits<size_t>::max());

	/**
	 * This method just takes ownership of any supplied types,
	 * which are destroyed when the chain gets destroyed.
	 * Useful for resource lifetime control.
	 */
	template <typename... Args>
	[[nodiscard]] Conveyor<T> attach(Args &&...args);

	/** @todo implement
	 * This method limits the total amount of passed elements
	 * Be careful where you place this node into the chain.
	 * If you meant to fork it and destroy paths you shouldn't place
	 * an interrupt point between the fork and this limiter
	 */
	[[nodiscard]] Conveyor<T> limit(size_t val = 1);

	/**
	 *
	 */
	[[nodiscard]] std::pair<Conveyor<T>, MergeConveyor<T>> merge();

	/**
	 * Moves the conveyor chain into a thread local storage point which drops
	 * every element. Use sink() if you want to control the lifetime of a
	 * conveyor chain
	 */
	template <typename ErrorFunc = PropagateError>
	void detach(ErrorFunc &&err_func = PropagateError());
	/**
	 * Creates a local sink which drops elements, but lifetime control remains
	 * in your hand.
	 */
	template <typename ErrorFunc = PropagateError>
	[[nodiscard]] SinkConveyor sink(ErrorFunc &&error_func = PropagateError());

	/**
	 * If no sink() or detach() is used you have to take elements out of the
	 * chain yourself.
	 */
	ErrorOr<FixVoid<T>> take();

	/** @todo implement
	 * Specifically pump elements through this chain
	 */
	void poll();

	// helper
	static Conveyor<T> toConveyor(Own<ConveyorNode> node,
								  ConveyorStorage *is_storage = nullptr);

	// helper
	static std::pair<Own<ConveyorNode>, ConveyorStorage *>
	fromConveyor(Conveyor<T> conveyor);
};

template <typename Func> ConveyorResult<Func, void> execLater(Func &&func);

/*
 * Join Conveyors into a single one
 */
template <typename... Args>
Conveyor<std::tuple<Args...>>
joinConveyors(std::tuple<Conveyor<Args>...> &conveyors);

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

/**
 * Class which acts as a correspondent between the running framework and outside
 * events which may be signals from the operating system or just other threads.
 * Default EventPorts are supplied by setupAsyncIo() in io.h
 */
class EventPort {
public:
	virtual ~EventPort() = default;

	virtual Conveyor<void> onSignal(Signal signal) = 0;

	virtual void poll() = 0;
	virtual void wait() = 0;
	virtual void wait(const std::chrono::steady_clock::duration &) = 0;
	virtual void wait(const std::chrono::steady_clock::time_point &) = 0;

	virtual void wake() = 0;
};

class SinkConveyorNode;

class ConveyorSinks final : public Event {
private:
	/*
		class Helper final : public Event {
		private:
			void destroySinkConveyorNode(ConveyorNode& sink);
			void fail(Error&& error);

			std::vector<Own<ConveyorNode>> sink_nodes;
			std::queue<ConveyorNode*> delete_nodes;
			std::function<void(Error&& error)> error_handler;

		public:
			ConveyorSinks() = default;
			ConveyorSinks(EventLoop& event_loop);

			void add(Conveyor<void> node);

			void fire() override {}
		};

		gin::Own<Helper> helper;
	*/
	friend class SinkConveyorNode;

	void destroySinkConveyorNode(ConveyorNode &sink_node);
	void fail(Error &&error);

	std::list<Own<ConveyorNode>> sink_nodes;

	std::queue<ConveyorNode *> delete_nodes;

	std::function<void(Error &&error)> error_handler;

public:
	// ConveyorSinks();
	// ConveyorSinks(EventLoop& event_loop);
	ConveyorSinks() = default;
	ConveyorSinks(EventLoop &event_loop);

	void add(Conveyor<void> &&node);

	void fire() override;
};

/*
 * EventLoop class similar to capn'proto.
 * https://github.com/capnproto/capnproto
 */
class EventLoop {
private:
	friend class Event;
	Event *head = nullptr;
	Event **tail = &head;
	Event **next_insert_point = &head;
	Event **later_insert_point = &head;

	bool is_runnable = false;

	Own<EventPort> event_port = nullptr;

	Own<ConveyorSinks> daemon_sink = nullptr;

	// functions
	void setRunnable(bool runnable);

	friend class WaitScope;
	void enterScope();
	void leaveScope();

	bool turnLoop();
	bool turn();

public:
	EventLoop();
	EventLoop(Own<EventPort> &&port);
	~EventLoop();

	EventLoop(EventLoop &&) = default;
	EventLoop &operator=(EventLoop &&) = default;

	bool wait();
	bool wait(const std::chrono::steady_clock::duration &);
	bool wait(const std::chrono::steady_clock::time_point &);
	bool poll();

	EventPort *eventPort();

	ConveyorSinks &daemon();
};

/*
 * WaitScope class similar to capn'proto.
 * https://github.com/capnproto/capnproto
 */
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
} // namespace saw

// Secret stuff
// Aka private semi hidden classes
namespace saw {

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
class AdaptConveyorFeeder final : public ConveyorFeeder<UnfixVoid<T>> {
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
class AdaptConveyorNode final : public ConveyorNode,
								public ConveyorEventStorage {
private:
	AdaptConveyorFeeder<T> *feeder = nullptr;

	std::queue<ErrorOr<UnfixVoid<T>>> storage;

public:
	AdaptConveyorNode();
	~AdaptConveyorNode();

	void setFeeder(AdaptConveyorFeeder<T> *feeder);

	void feed(T &&value);
	void fail(Error &&error);

	// ConveyorNode
	void getResult(ErrorOrValue &err_or_val) override;

	// ConveyorStorage
	size_t space() const override;
	size_t queued() const override;

	void childHasFired() override;
	void parentHasFired() override;

	// Event
	void fire() override;
};

template <typename T> class OneTimeConveyorNode;

template <typename T>
class OneTimeConveyorFeeder final : public ConveyorFeeder<UnfixVoid<T>> {
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
class OneTimeConveyorNode final : public ConveyorNode,
								  public ConveyorStorage,
								  public Event {
protected:
	Own<ConveyorNode> child;

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

	void childHasFired() override {}
	void parentHasFired() override;

	// Event
	void fire() override;
};

class QueueBufferConveyorNodeBase : public ConveyorNode,
									public ConveyorEventStorage {
protected:
	Own<ConveyorNode> child;

public:
	QueueBufferConveyorNodeBase(ConveyorStorage *child_store,
								Own<ConveyorNode> dep)
		: ConveyorEventStorage{child_store}, child(std::move(dep)) {}
	virtual ~QueueBufferConveyorNodeBase() = default;
};

template <typename T>
class QueueBufferConveyorNode final : public QueueBufferConveyorNodeBase {
private:
	std::queue<ErrorOr<T>> storage;
	size_t max_store;

public:
	QueueBufferConveyorNode(ConveyorStorage *child_store, Own<ConveyorNode> dep,
							size_t max_size)
		: QueueBufferConveyorNodeBase{child_store, std::move(dep)},
		  max_store{max_size} {}
	// Event
	void fire() override;
	// ConveyorNode
	void getResult(ErrorOrValue &eov) noexcept override;

	// ConveyorStorage
	size_t space() const override;
	size_t queued() const override;

	void childHasFired() override;
	void parentHasFired() override;
};

class AttachConveyorNodeBase : public ConveyorNode {
protected:
	Own<ConveyorNode> child;

public:
	AttachConveyorNodeBase(Own<ConveyorNode> &&dep) : child(std::move(dep)) {}

	virtual ~AttachConveyorNodeBase() = default;

	void getResult(ErrorOrValue &err_or_val) noexcept override;
};

template <typename... Args>
class AttachConveyorNode final : public AttachConveyorNodeBase {
private:
	std::tuple<Args...> attached_data;

public:
	AttachConveyorNode(Own<ConveyorNode> &&dep, Args &&...args)
		: AttachConveyorNodeBase(std::move(dep)), attached_data{
													  std::move(args...)} {}
};

class ConvertConveyorNodeBase : public ConveyorNode {
protected:
	Own<ConveyorNode> child;

public:
	ConvertConveyorNodeBase(Own<ConveyorNode> &&dep);
	virtual ~ConvertConveyorNodeBase() = default;

	void getResult(ErrorOrValue &err_or_val) override;

	virtual void getImpl(ErrorOrValue &err_or_val) = 0;
};

template <typename T, typename DepT, typename Func, typename ErrorFunc>
class ConvertConveyorNode final : public ConvertConveyorNodeBase {
private:
	Func func;
	ErrorFunc error_func;

	static_assert(std::is_same<DepT, RemoveErrorOr<DepT>>::value,
				  "Should never be of type ErrorOr");

public:
	ConvertConveyorNode(Own<ConveyorNode> &&dep, Func &&func,
						ErrorFunc &&error_func)
		: ConvertConveyorNodeBase(std::move(dep)), func{std::move(func)},
		  error_func{std::move(error_func)} {}

	void getImpl(ErrorOrValue &err_or_val) noexcept override {
		ErrorOr<UnfixVoid<DepT>> dep_eov;
		ErrorOr<UnfixVoid<RemoveErrorOr<T>>> &eov =
			err_or_val.as<UnfixVoid<RemoveErrorOr<T>>>();
		if (child) {
			child->getResult(dep_eov);
			if (dep_eov.isValue()) {
				try {

					eov = FixVoidCaller<T, DepT>::apply(
						func, std::move(dep_eov.value()));
				} catch (const std::bad_alloc &) {
					eov = criticalError("Out of memory");
				} catch (const std::exception &) {
					eov = criticalError(
						"Exception in chain occured. Return ErrorOr<T> if you "
						"want to handle errors which are recoverable");
				}
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

class SinkConveyorNode final : public ConveyorNode,
							   public ConveyorEventStorage {
private:
	Own<ConveyorNode> child;
	ConveyorSinks *conveyor_sink;

public:
	SinkConveyorNode(ConveyorStorage *child_store, Own<ConveyorNode> node,
					 ConveyorSinks &conv_sink)
		: ConveyorEventStorage{child_store}, child{std::move(node)},
		  conveyor_sink{&conv_sink} {}

	SinkConveyorNode(ConveyorStorage *child_store, Own<ConveyorNode> node)
		: ConveyorEventStorage{child_store}, child{std::move(node)},
		  conveyor_sink{nullptr} {}

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
	void getResult(ErrorOrValue &err_or_val) noexcept override {
		err_or_val.as<Void>() =
			criticalError("In a sink node no result can be returned");
	}

	// ConveyorStorage
	void childHasFired() override {
		if (child) {
			ErrorOr<void> dep_eov;
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

	/*
	 * No parent needs to be fired since we always have space
	 */
	void parentHasFired() override {}
};

class ImmediateConveyorNodeBase : public ConveyorNode,
								  public ConveyorEventStorage {
private:
public:
	ImmediateConveyorNodeBase();

	virtual ~ImmediateConveyorNodeBase() = default;
};

template <typename T>
class ImmediateConveyorNode final : public ImmediateConveyorNodeBase {
private:
	ErrorOr<FixVoid<T>> value;
	uint8_t retrieved;

public:
	ImmediateConveyorNode(FixVoid<T> &&val);
	ImmediateConveyorNode(Error &&error);

	// ConveyorStorage
	size_t space() const override;
	size_t queued() const override;

	void childHasFired() override;
	void parentHasFired() override;

	// ConveyorNode
	void getResult(ErrorOrValue &err_or_val) noexcept override {
		if (retrieved > 0) {
			err_or_val.as<FixVoid<T>>() =
				makeError("Already taken value", Error::Code::Exhausted);
		} else {
			err_or_val.as<FixVoid<T>>() = std::move(value);
		}
		if (queued() > 0) {
			++retrieved;
		}
	}

	// Event
	void fire() override;
};

/*
 * Collects every incoming value and throws it in one lane
 */
class MergeConveyorNodeBase : public ConveyorNode, public ConveyorEventStorage {
public:
	MergeConveyorNodeBase();

	virtual ~MergeConveyorNodeBase() = default;
};

template <typename T> class MergeConveyorNode : public MergeConveyorNodeBase {
private:
	class Appendage final : public ConveyorStorage {
	public:
		Own<ConveyorNode> child;
		MergeConveyorNode *merger;

		Maybe<ErrorOr<FixVoid<T>>> error_or_value;

	public:
		Appendage(ConveyorStorage *child_store, Own<ConveyorNode> n,
				  MergeConveyorNode &m)
			: ConveyorStorage{child_store}, child{std::move(n)}, merger{&m},
			  error_or_value{std::nullopt} {}

		bool childStorageHasElementQueued() const {
			if (child_storage) {
				return child_storage->queued() > 0;
			}
			return false;
		}

		void getAppendageResult(ErrorOrValue &eov);

		size_t space() const override;

		size_t queued() const override;

		void childHasFired() override;

		void parentHasFired() override;

		void setParent(ConveyorStorage *par) override;
	};

	friend class MergeConveyorNodeData<T>;
	friend class Appendage;

	Our<MergeConveyorNodeData<T>> data;
	size_t next_appendage = 0;

public:
	MergeConveyorNode(Our<MergeConveyorNodeData<T>> data);
	~MergeConveyorNode();

	// Event
	void getResult(ErrorOrValue &err_or_val) noexcept override;

	void fire() override;

	// ConveyorStorage

	size_t space() const override;
	size_t queued() const override;
	void childHasFired() override;
	void parentHasFired() override;
};

template <typename T> class MergeConveyorNodeData {
public:
	std::vector<Own<typename MergeConveyorNode<T>::Appendage>> appendages;

	MergeConveyorNode<T> *merger = nullptr;

public:
	void attach(Conveyor<T> conv);

	void governingNodeDestroyed();
};

/*
class JoinConveyorNodeBase : public ConveyorNode, public ConveyorEventStorage {
private:

public:
};

template <typename... Args>
class JoinConveyorNode final : public JoinConveyorNodeBase {
private:
	template<typename T>
	class Appendage : public ConveyorEventStorage {
	private:
		Maybe<T> data = std::nullopt;

	public:
		size_t space() const override;
		size_t queued() const override;

		void fire() override;
		void getResult(ErrorOrValue& eov) override;
	};

	std::tuple<Appendage<Args>...> appendages;

public:
};

*/

} // namespace saw

#include "async.tmpl.h"
