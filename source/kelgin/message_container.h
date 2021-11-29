#pragma once

#include "schema.h"

namespace gin {
template<class T>
class MessageContainer;

template <class T, class Container> class Message;

template <size_t N, class... T> struct MessageParameterPackType ;

template <class TN, class... T> struct MessageParameterPackType <0, TN, T...> {
	using Type = T;
};

template <size_t N, class TN, class... T> struct MessageParameterPackType <N, TN, T...> {
	using Type = typename MessageParameterPackType<N-1, T...>::Type;
};

template<class T, class... TL> struct MessageParameterPackIndex;

template<class T, class... TL> struct MessageParameterPackIndex<T,T, TL...> {
	static constexpr size_t Value = 0u;
};

template<class T, class TL0, class... TL> struct MessageParameterPackIndex<T, TL0, TL...> {
	static constexpr size_t Value = 1u + MessageParameterPackIndex<T, TL...>::Value;
};

template<class... V, class... K> class MessageContainer<schema::Struct<schema::NamedMember<V,K>...>> {
private:
	using ValueType = std::tuple<Message<V, MessageContainer<V>>...>;
	ValueType values;
public:
	using SchemaType = schema::Struct<schema::NamedMember<V,K>...>;

	template<size_t i>
	using Element = MessageParameterPackType<N, Message<V,MessageContainer<V>>...>;

	template<size_t i>
	Element<i>& get(){
		return std::get<i>(values);
	}
};
}
