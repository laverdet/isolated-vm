module;
#include <utility>
export module isolated_js:recursive_value;

namespace js {

// Self-referential value type which can refer to itself by means of a template.
export template <template <class> class Make>
class recursive_value : public Make<recursive_value<Make>> {
	public:
		using value_type = Make<recursive_value<Make>>;
		explicit constexpr recursive_value(const value_type& value) : value_type{value} {}
		explicit constexpr recursive_value(value_type&& value) : value_type{std::move(value)} {}
};

// Concept for any `recursive_value` instantiation.
export template <class Type>
concept is_recursive_value = requires {
	[]<template <class> class Make>(const recursive_value<Make>&) {}(std::declval<Type>());
};

} // namespace js
