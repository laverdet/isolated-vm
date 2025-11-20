module;
#include <concepts>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>
export module napi_js:accept;
import :api;
import :container;
import :dictionary; // ?? (`:value` should import it)
import :environment_fwd;
import :value;
import util;

namespace js {
using namespace napi;

// Generic napi acceptor which accepts all value types.
template <class Environment>
struct accept_napi_value;

template <class Meta>
using accept_napi_value_with = accept_napi_value<typename Meta::accept_context_type>;

template <class Environment>
struct accept_napi_value : napi::environment_scope<Environment> {
	public:
		using napi::environment_scope<Environment>::environment;
		using accept_reference_type = napi_value;

		explicit accept_napi_value(auto* /*transfer*/, auto& env) :
				napi::environment_scope<Environment>{env} {}

		// reference provider
		constexpr auto operator()(std::type_identity<napi_value> /*type*/, napi_value value) const -> napi_value { return value; }

		template <class Tag>
		constexpr auto operator()(std::type_identity<napi::value<Tag>> /*type*/, napi_value value) const -> napi::value<Tag> {
			return napi::value<Tag>::from(value);
		}

		// undefined & null
		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*undefined*/) const -> napi::value<undefined_tag> {
			return napi::value<undefined_tag>::make(environment(), std::monostate{});
		}

		auto operator()(null_tag /*tag*/, visit_holder /*visit*/, const auto& /*null*/) const -> napi::value<null_tag> {
			return napi::value<null_tag>::make(environment(), nullptr);
		}

		// boolean
		auto operator()(boolean_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const -> napi::value<boolean_tag> {
			return napi::value<boolean_tag>::make(environment(), bool{std::forward<decltype(subject)>(subject)});
		}

		// number
		auto operator()(number_tag /*tag*/, visit_holder visit, auto&& subject) const -> napi::value<number_tag> {
			return (*this)(number_tag_of<double>{}, visit, std::forward<decltype(subject)>(subject));
		}

		template <class Numeric>
		auto operator()(number_tag_of<Numeric> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> napi::value<number_tag> {
			return napi::value<number_tag>::make(environment(), Numeric{std::forward<decltype(subject)>(subject)});
		}

		// bigint
		auto operator()(bigint_tag /*tag*/, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<napi::value<bigint_tag>> {
			return (*this)(bigint_tag_of<bigint>{}, visit, std::forward<decltype(subject)>(subject));
		}

		auto operator()(bigint_tag_of<bigint> /*tag*/, visit_holder /*visit*/, auto&& subject) const
			-> js::referenceable_value<napi::value<bigint_tag>> {
			return js::referenceable_value{napi::value<bigint_tag>::make(environment(), bigint{std::forward<decltype(subject)>(subject)})};
		}

		auto operator()(bigint_tag_of<bigint> /*tag*/, visit_holder /*visit*/, const bigint& value) const
			-> js::referenceable_value<napi::value<bigint_tag>> {
			return js::referenceable_value{napi::value<bigint_tag>::make(environment(), value)};
		}

		template <class Numeric>
		auto operator()(bigint_tag_of<Numeric> /*tag*/, visit_holder /*visit*/, auto&& subject) const
			-> js::referenceable_value<napi::value<bigint_tag>> {
			return js::referenceable_value{napi::value<bigint_tag>::make(environment(), Numeric{std::forward<decltype(subject)>(subject)})};
		}

		// string
		auto operator()(string_tag /*tag*/, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<napi::value<string_tag>> {
			return (*this)(string_tag_of<char16_t>{}, visit, std::forward<decltype(subject)>(subject));
		}

		template <class Char>
		auto operator()(string_tag_of<Char> /*tag*/, visit_holder /*visit*/, std::convertible_to<std::basic_string_view<Char>> auto&& subject) const
			-> js::referenceable_value<napi::value<string_tag>> {
			return js::referenceable_value{napi::value<string_tag>::make(environment(), std::basic_string_view<Char>{std::forward<decltype(subject)>(subject)})};
		}

		template <class Char>
		auto operator()(string_tag_of<Char> tag, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<napi::value<string_tag>> {
			return (*this)(tag, visit, std::basic_string<Char>{std::forward<decltype(subject)>(subject)});
		}

		// date
		auto operator()(date_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const
			-> js::referenceable_value<napi::value<date_tag>> {
			return js::referenceable_value{napi::value<date_tag>::make(environment(), js_clock::time_point{std::forward<decltype(subject)>(subject)})};
		}

		// error
		auto operator()(error_tag /*tag*/, visit_holder /*visit*/, const auto& subject) const
			-> js::referenceable_value<napi::value<error_tag>> {
			auto* message = napi_value{napi::value<string_tag>::make(environment(), subject.message())};
			auto error = [ & ]() -> napi_value {
				switch (subject.name()) {
					default:
						return napi::invoke(napi_create_error, napi_env{*this}, napi_value{}, message);
					case js::error::name_type::range_error:
						return napi::invoke(napi_create_range_error, napi_env{*this}, napi_value{}, message);
					case js::error::name_type::syntax_error:
						return napi::invoke(node_api_create_syntax_error, napi_env{*this}, napi_value{}, message);
					case js::error::name_type::type_error:
						return napi::invoke(napi_create_type_error, napi_env{*this}, napi_value{}, message);
				}
			}();
			return js::referenceable_value{napi::value<error_tag>::from(error)};
		}

		// vectors
		auto operator()(this const auto& self, vector_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<napi::value<vector_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				napi::value<vector_tag>::from(napi::invoke(napi_create_array_with_length, napi_env{self}, subject.size())),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](napi::value<vector_tag> array, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					int ii = 0;
					auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
					for (auto&& subject : util::forward_range(std::forward<decltype(range)>(range))) {
						auto* element = napi_value{visit(std::forward<decltype(subject)>(subject), self)};
						napi::invoke0(napi_set_element, napi_env{self}, napi_value{array}, ii++, element);
					}
				},
			};
		}

		template <std::size_t Size>
		auto operator()(this const auto& self, tuple_tag<Size> /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<napi::value<vector_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				napi::value<vector_tag>::from(napi::invoke(napi_create_array_with_length, napi_env{self}, Size)),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](napi::value<vector_tag> array, auto& self, auto& visit, auto /*&&*/ tuple) -> void {
					const auto [... indices ] = util::sequence<Size>;
					(..., [ & ]() -> void {
						// nb: This is forwarded to *each* visitor. The visitor should be aware and only lvalue
						// reference members one at a time.
						auto* element = napi_value{visit(indices, std::forward<decltype(tuple)>(tuple), self)};
						napi::invoke0(napi_set_element, napi_env{self}, napi_value{array}, indices, element);
					}());
				}
			};
		}

		// arrays & dictionaries
		auto operator()(this const auto& self, list_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<napi::value<list_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				napi::value<list_tag>::from(napi::invoke(napi_create_array, napi_env{self})),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](napi::value<list_tag> array, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					std::vector<napi_property_descriptor> properties;
					auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
					properties.reserve(std::size(range));
					for (auto&& [ key, value ] : util::forward_range(std::forward<decltype(range)>(range))) {
						properties.emplace_back(napi_property_descriptor{
							.utf8name{},
							.name = napi_value{visit.first(std::forward<decltype(key)>(key), self)},

							.method{},
							.getter{},
							.setter{},
							.value = napi_value{visit.second(std::forward<decltype(value)>(value), self)},

							// NOLINTNEXTLINE(hicpp-signed-bitwise)
							.attributes = static_cast<napi_property_attributes>(napi_writable | napi_enumerable | napi_configurable),
							.data{},
						});
					}
					napi::invoke0(napi_define_properties, napi_env{self}, napi_value{array}, properties.size(), properties.data());
				}
			};
		}

		auto operator()(this const auto& self, dictionary_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<napi::value<dictionary_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				napi::value<dictionary_tag>::from(napi::invoke(napi_create_object, napi_env{self})),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](napi::value<dictionary_tag> object, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					std::vector<napi_property_descriptor> properties;
					auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
					properties.reserve(std::size(range));
					for (auto&& [ key, value ] : util::forward_range(std::forward<decltype(range)>(range))) {
						properties.emplace_back(napi_property_descriptor{
							.utf8name{},
							.name = napi_value{visit.first(std::forward<decltype(key)>(key), self)},

							.method{},
							.getter{},
							.setter{},
							.value = napi_value{visit.second(std::forward<decltype(value)>(value), self)},

							// NOLINTNEXTLINE(hicpp-signed-bitwise)
							.attributes = static_cast<napi_property_attributes>(napi_writable | napi_enumerable | napi_configurable),
							.data{},
						});
					}
					napi::invoke0(napi_define_properties, napi_env{self}, napi_value{object}, properties.size(), properties.data());
				}
			};
		}

		// structs
		template <std::size_t Size>
		auto operator()(this const auto& self, struct_tag<Size> /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<napi::value<dictionary_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				napi::value<dictionary_tag>::from(napi::invoke(napi_create_object, napi_env{self})),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](napi::value<dictionary_tag> object, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					std::array<napi_property_descriptor, Size> properties;
					const auto [... indices ] = util::sequence<Size>;
					(..., [ & ]() -> void {
						auto& visit_n = std::get<indices>(visit);
						properties[ indices ] = {
							.utf8name{},
							.name = napi_value{visit_n.first.get_local(self)},

							.method{},
							.getter{},
							.setter{},
							// nb: This is forwarded to *each* visitor. The visitor should be aware and only lvalue
							// reference members one at a time.
							.value = napi_value{visit_n.second(std::forward<decltype(subject)>(subject), self)},

							// NOLINTNEXTLINE(hicpp-signed-bitwise)
							.attributes = static_cast<napi_property_attributes>(napi_writable | napi_enumerable | napi_configurable),
							.data{},
						};
					}());
					napi::invoke0(napi_define_properties, napi_env{self}, napi_value{object}, properties.size(), properties.data());
				}
			};
		}
};

// Plain napi_value acceptor
template <>
struct accept_property_subject<napi_value> : std::type_identity<napi_value> {};

template <class Meta>
struct accept<Meta, napi_value> : accept_napi_value_with<Meta> {
		using accept_type = accept_napi_value_with<Meta>;
		using accept_type::accept_type;
};

// Tagged `napi::value<T>` acceptor
template <>
struct accept_property_subject<napi::value<value_tag>> : std::type_identity<napi_value> {};

template <class Meta>
struct accept<Meta, napi::value<value_tag>> : accept_napi_value_with<Meta> {
		using accept_type = accept_napi_value_with<Meta>;
		using accept_type::accept_type;
};

// Forwarded `napi::value<T>` acceptor
template <class Tag>
struct accept<void, js::forward<napi::value<Tag>, Tag>> {
		auto operator()(Tag /*tag*/, visit_holder /*visit*/, napi::value<Tag> value) const -> js::forward<napi::value<Tag>, Tag> {
			return js::forward{napi::value<Tag>{value}, Tag{}};
		}
};

// Object key lookup via napi
template <class Meta, auto Key, class Type>
struct accept_property_value<Meta, Key, Type, napi_value> {
	public:
		explicit constexpr accept_property_value(auto* transfer) :
				second{transfer} {}

		auto operator()(dictionary_tag /*tag*/, auto& visit, const auto& object) const -> Type {
			if (auto local = first.get_local(visit.first); object.has(local)) {
				return visit.second(object.get(local), second);
			} else {
				return second(undefined_in_tag{}, visit.second, std::monostate{});
			}
		}

	private:
		mutable visit_key_literal<Key, napi_value> first;
		accept_value<Meta, Type> second;
};

} // namespace js
