#pragma once

#include "common.h"

#include <cassert>
// Template inlining
namespace gin {

template <typename Func> ConveyorResult<Func, void> execLater(Func &&func) {
	Conveyor<void> conveyor{FixVoid<void>{}};
	return conveyor.then(std::move(func));
}

template <typename T>
Conveyor<T>::Conveyor(FixVoid<T> value) : ConveyorBase(nullptr, nullptr) {
	// Is there any way to do  this?
	// @todo new ConveyorBase constructor for Immediate values

	Own<ImmediateConveyorNode<FixVoid<T>>> immediate =
		heap<ImmediateConveyorNode<FixVoid<T>>>(std::move(value));

	if (!immediate) {
		return;
	}

	storage = static_cast<ConveyorStorage *>(immediate.get());
	node = std::move(immediate);
}

template <typename T>
Conveyor<T>::Conveyor(Error &&error) : ConveyorBase(nullptr, nullptr) {
	Own<ImmediateConveyorNode<FixVoid<T>>> immediate =
		heap<ImmediateConveyorNode<FixVoid<T>>>(std::move(error));

	if (!immediate) {
		return;
	}

	storage = static_cast<ConveyorStorage *>(immediate.get());
	node = std::move(immediate);
}

template <typename T>
Conveyor<T>::Conveyor(Own<ConveyorNode> node_p, ConveyorStorage *storage_p)
	: ConveyorBase{std::move(node_p), storage_p} {}

template <typename T>
template <typename Func, typename ErrorFunc>
ConveyorResult<Func, T> Conveyor<T>::then(Func &&func, ErrorFunc &&error_func) {
	Own<ConveyorNode> conversion_node =
		heap<ConvertConveyorNode<FixVoid<ReturnType<Func, T>>, FixVoid<T>, Func,
								 ErrorFunc>>(std::move(node), std::move(func),
											 std::move(error_func));

	return Conveyor<RemoveErrorOr<ReturnType<Func, T>>>::toConveyor(
		std::move(conversion_node), storage);
}

template <typename T> Conveyor<T> Conveyor<T>::buffer(size_t size) {
	Own<QueueBufferConveyorNode<FixVoid<T>>> storage_node =
		heap<QueueBufferConveyorNode<FixVoid<T>>>(storage, std::move(node),
												  size);
	ConveyorStorage *storage_ptr =
		static_cast<ConveyorStorage *>(storage_node.get());
	GIN_ASSERT(storage) { return Conveyor<T>{nullptr, nullptr}; }

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

template <typename T>
std::pair<Conveyor<T>, MergeConveyor<T>> Conveyor<T>::merge() {
	Our<MergeConveyorNodeData<T>> data = share<MergeConveyorNodeData<T>>();

	Own<MergeConveyorNode<T>> merge_node = heap<MergeConveyorNode<T>>(data);

	data->attach(Conveyor<T>::toConveyor(std::move(node), storage));

	MergeConveyor<T> node_ref{data};

	return std::make_pair(Conveyor<T>{std::move(merge_node), storage},
						  std::move(node_ref));
}

template <>
template <typename ErrorFunc>
SinkConveyor Conveyor<void>::sink(ErrorFunc &&error_func) {
	Own<SinkConveyorNode> sink_node =
		heap<SinkConveyorNode>(storage, std::move(node));
	ConveyorStorage *storage_ptr =
		static_cast<ConveyorStorage *>(sink_node.get());

	GIN_ASSERT(storage) { return SinkConveyor{}; }
	storage->setParent(storage_ptr);

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
Conveyor<T> Conveyor<T>::toConveyor(Own<ConveyorNode> node,
									ConveyorStorage *storage) {
	return Conveyor<T>{std::move(node), storage};
}

template <typename T>
std::pair<Own<ConveyorNode>, ConveyorStorage *>
Conveyor<T>::fromConveyor(Conveyor<T> conveyor) {
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

// QueueBuffer
template <typename T> void QueueBufferConveyorNode<T>::fire() {
	if (child) {
		if (!storage.empty()) {
			if (storage.front().isError()) {
				if (storage.front().error().isCritical()) {
					child = nullptr;
					child_storage = nullptr;
				}
			}
		}
	}

	bool has_space_before_fire = space() > 0;

	if (parent) {
		parent->childHasFired();
		if (!storage.empty() && parent->space() > 0) {
			armLater();
		}
	}

	if (child_storage && !has_space_before_fire) {
		child_storage->parentHasFired();
	}
}

template <typename T>
void QueueBufferConveyorNode<T>::getResult(ErrorOrValue &eov) {
	ErrorOr<T> &err_or_val = eov.as<T>();
	err_or_val = std::move(storage.front());
	storage.pop();
}

template <typename T> void QueueBufferConveyorNode<T>::parentHasFired() {
	GIN_ASSERT(parent) { return; }

	if (parent->space() == 0) {
		return;
	}

	if (queued() > 0) {
		armLater();
	}
}

template <typename T>
ImmediateConveyorNode<T>::ImmediateConveyorNode(FixVoid<T> &&val)
	: value{std::move(val)}, retrieved{0} {}

template <typename T>
ImmediateConveyorNode<T>::ImmediateConveyorNode(Error &&error)
	: value{std::move(error)}, retrieved{0} {}

template <typename T> size_t ImmediateConveyorNode<T>::space() const {
	return 0;
}

template <typename T> size_t ImmediateConveyorNode<T>::queued() const {
	return retrieved > 1 ? 0 : 1;
}

template <typename T> void ImmediateConveyorNode<T>::childHasFired() {
	// Impossible case
	assert(false);
}

template <typename T> void ImmediateConveyorNode<T>::parentHasFired() {
	GIN_ASSERT(parent) { return; }
	assert(parent->space() > 0);

	if (queued() > 0) {
		armNext();
	}
}

template <typename T> void ImmediateConveyorNode<T>::fire() {
	if (parent) {
		parent->childHasFired();
		if (queued() > 0 && parent->space() > 0) {
			armLast();
		}
	}
}

template <typename T>
MergeConveyor<T>::MergeConveyor(Lent<MergeConveyorNodeData<T>> d)
	: data{std::move(d)} {}

template <typename T> MergeConveyor<T>::~MergeConveyor() {}

template <typename T> void MergeConveyor<T>::attach(Conveyor<T> conveyor) {
	auto sp = data.lock();
	GIN_ASSERT(sp) { return; }

	sp->attach(std::move(conveyor));
}

template <typename T>
MergeConveyorNode<T>::MergeConveyorNode(Our<MergeConveyorNodeData<T>> d)
	: data{d} {
	GIN_ASSERT(data) { return; }

	data->merger = this;
}

template <typename T> MergeConveyorNode<T>::~MergeConveyorNode() {}

template <typename T> void MergeConveyorNode<T>::getResult(ErrorOrValue &eov) {
	ErrorOr<T> &err_or_val = eov.as<T>();

	if (error_or_value.has_value()) {
		err_or_val = std::move(error_or_value.value());
		error_or_value = std::nullopt;
	} else {
		error_or_value = criticalError("No value in MergeConveyorNode");
	}
}

template <typename T> void MergeConveyorNode<T>::fire() {
	GIN_ASSERT(queued() > 0) { return; }

	if (parent) {
		parent->childHasFired();
	}

	if (queued() > 0 && parent->space() > 0) {
		armLater();
	} else {
		/// @unimplemented search for arm which has remaining elements
	}
}

template <typename T> size_t MergeConveyorNode<T>::space() const {
	return error_or_value.has_value() ? 0 : 1;
}

template <typename T> size_t MergeConveyorNode<T>::queued() const {
	return error_or_value.has_value() ? 1 : 0;
}

template <typename T> void MergeConveyorNode<T>::childHasFired() {
	/// This can never happen
	assert(false);
}

template <typename T> void MergeConveyorNode<T>::parentHasFired() {
	GIN_ASSERT(parent) { return; }
	if (queued() > 0) {
		if (parent->space() > 0) {
			armLater();
		}
	}
}

template <typename T> size_t MergeConveyorNode<T>::Appendage::space() const {
	GIN_ASSERT(merger) { return 0; }

	if (merger->error_or_value.has_value()) {
		return 0;
	}

	return 1;
}

template <typename T> size_t MergeConveyorNode<T>::Appendage::queued() const {
	GIN_ASSERT(merger) { return 0; }

	if (merger->error_or_value.has_value()) {
		return 1;
	}

	return 0;
}

template <typename T> void MergeConveyorNode<T>::Appendage::childHasFired() {
	GIN_ASSERT(!merger->error_or_value.has_value()) { return; }
	ErrorOr<FixVoid<T>> eov;
	child->getResult(eov);

	merger->error_or_value = std::move(eov);
}

template <typename T> void MergeConveyorNode<T>::Appendage::parentHasFired() {
	if (child_storage) {
		child_storage->parentHasFired();
	}
}

template <typename T>
void MergeConveyorNode<T>::Appendage::setParent(ConveyorStorage *par) {
	GIN_ASSERT(merger) { return; }

	GIN_ASSERT(child) { return; }

	parent = par;
}

template <typename T>
void MergeConveyorNodeData<T>::attach(Conveyor<T> conveyor) {
	auto nas = Conveyor<T>::fromConveyor(std::move(conveyor));

	auto merge_node_appendage = heap<typename MergeConveyorNode<T>::Appendage>(
		nas.second, std::move(nas.first), *merger);

	if (nas.second) {
		nas.second->setParent(merge_node_appendage.get());
	}

	appendages.push_back(std::move(merge_node_appendage));
}

template <typename T> void MergeConveyorNodeData<T>::notifyNextAppendage() {
	next_appendage = std::min(next_appendage, appendages.size());

	for (size_t i = next_appendage; i < appendages.size(); ++i) {
		if (appendages[i]->childStorageHasElementQueued()) {
			appendages[i]->parentHasFired();
			next_appendage = i + 1;
			return;
		}
	}

	for (size_t i = 0; i < next_appendage; ++i) {
		if (appendages[i]->childStorageHasElementQueued()) {
			appendages[i]->parentHasFired();
			next_appendage = i + 1;
			return;
		}
	}
}

template <typename T> void MergeConveyorNodeData<T>::governingNodeDestroyed() {
	appendages.clear();
	merger = nullptr;
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

template <typename T>
AdaptConveyorNode<T>::AdaptConveyorNode() : ConveyorEventStorage{nullptr} {}

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

template <typename T> void AdaptConveyorNode<T>::childHasFired() {
	// Adapt node has no children
	assert(false);
}

template <typename T> void AdaptConveyorNode<T>::parentHasFired() {
	GIN_ASSERT(parent) { return; }

	if (parent->space() == 0) {
		return;
	}
}

template <typename T> void AdaptConveyorNode<T>::fire() {
	if (parent) {
		parent->childHasFired();

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
		parent->childHasFired();
	}
}

} // namespace gin
