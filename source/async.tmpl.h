#pragma once

#include <cassert>
// Template inlining
namespace gin {

template <typename T>
ImmediateConveyorNode<T>::ImmediateConveyorNode(FixVoid<T> &&val)
	: value{std::move(val)}, retrieved{false} {}

template <typename T>
ImmediateConveyorNode<T>::ImmediateConveyorNode(Error &&error)
	: value{std::move(error)}, retrieved{false} {}

template <typename T> size_t ImmediateConveyorNode<T>::space() const {
	return 0;
}

template <typename T> size_t ImmediateConveyorNode<T>::queued() const {
	return retrieved ? 0 : 1;
}

template <typename T> void ImmediateConveyorNode<T>::childFired() {
	// Impossible case
	assert(false);
}

template <typename T> void ImmediateConveyorNode<T>::fire() {
	if (parent) {
		parent->childFired();
	}
}

template <typename T> T reduceErrorOrType(T *);

template <typename T> T reduceErrorOrType(ErrorOr<T> *);

template <typename T>
using ReduceErrorOr = decltype(reduceErrorOrType((T *)nullptr));

template <typename T>
Conveyor<T>::Conveyor(FixVoid<T> value) : ConveyorBase(nullptr, nullptr) {
	// Is there any way to do  this?
	// @todo new ConveyorBase constructor for Immediate values

	Own<ImmediateConveyorNode<FixVoid<T>>> immediate =
		heap<ImmediateConveyorNode<FixVoid<T>>>(std::move(value));

	if (!immediate) {
		return;
	}

	storage = reinterpret_cast<ConveyorStorage *>(immediate.get());
	node = std::move(immediate);
}

template <typename T>
Conveyor<T>::Conveyor(Error &&error) : ConveyorBase(nullptr, nullptr) {
	Own<ImmediateConveyorNode<FixVoid<T>>> immediate =
		heap<ImmediateConveyorNode<FixVoid<T>>>(std::move(error));

	if (!immediate) {
		return;
	}

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
Conveyor<T> Conveyor<T>::attach(Args &&...args) {
	Own<AttachConveyorNode<Args...>> attach_node =
		heap<AttachConveyorNode<Args...>>(std::move(node), std::move(args...));
	return Conveyor<T>{std::move(attach_node), storage};
}

template <>
template <typename ErrorFunc>
SinkConveyor Conveyor<void>::sink(ErrorFunc &&error_func) {
	Own<SinkConveyorNode> sink_node = heap<SinkConveyorNode>(std::move(node));
	ConveyorStorage *storage_ptr =
		static_cast<ConveyorStorage *>(sink_node.get());
	if (storage) {
		storage->setParent(storage_ptr);
	}
	return SinkConveyor{std::move(sink_node)};
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
			return result;
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
