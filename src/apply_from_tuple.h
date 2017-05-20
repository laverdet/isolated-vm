#pragma once
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

// adapted from: http://www.cppsamples.com/common-tasks/apply-tuple-to-function.html

template<typename F, typename Tuple, size_t ...S>
decltype(auto) apply_tuple_impl(F&& fn, Tuple&& t, std::index_sequence<S...>) {
	return std::forward<F>(fn)(std::get<S>(std::forward<Tuple>(t))...);
}

template<typename F, typename ...Tuple>
decltype(auto) apply_from_tuple(F&& fn, std::tuple<Tuple...> t) {
	std::size_t constexpr tSize = std::tuple_size<typename std::remove_reference<std::tuple<Tuple...>>::type>::value;
	return apply_tuple_impl(std::forward<F>(fn), std::forward<std::tuple<Tuple...>>(t), std::make_index_sequence<tSize>());
}

template<typename F, typename T>
decltype(auto) apply_from_tuple(F&& fn, T&& t) {
	return fn(std::forward<T>(t));
}
