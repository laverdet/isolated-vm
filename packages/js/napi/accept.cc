module;
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
export module napi_js:accept;
import :api;
import :bound_value;
import :dictionary;
import :environment;
import :primitive;
import :value;
import isolated_js;
import ivm.utility;

namespace js {
using namespace napi;

// Helper which accepts property names and uses `make_property_name` to make an internalized property name
template <class Environment, class Type>
struct accept_napi_property_name : napi::environment_scope<Environment> {
		using accept_target_type = Type;

		explicit accept_napi_property_name(const auto& scope, const auto& /*accept*/) :
				napi::environment_scope<Environment>{scope} {}

		template <auto_tag Tag>
		auto operator()(Tag /*tag*/, visit_holder /*visit*/, auto&& value) const -> napi::value<primitive_tag> {
			return napi::value<Tag>::make_property_name(this->environment(), std::forward<decltype(value)>(value));
		}
};

// TODO: This is messy. `accept` needs to be passed twice, once to infer `Environment` and again to
// infer `Type`. I couldn't get clang to accept a seemingly well-formed generic deduction guide
// through one param. This probably isn't the way I want to do properties in the long term, though.
template <class Environment, class Accept>
accept_napi_property_name(napi::environment_scope<Environment>, Accept) -> accept_napi_property_name<Environment, accept_target_t<Accept>>;

// Non-recursive primitive / intrinsic acceptor which returns varying `value<T>` types depending on
// the subject.
template <class Meta, class Environment>
struct accept_napi_value : napi::environment_scope<Environment> {
	public:
		explicit accept_napi_value(auto* /*transfer*/, auto& env) :
				napi::environment_scope<Environment>{env} {}

		// undefined & null
		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*undefined*/) const -> napi::value<undefined_tag> {
			return napi::value<undefined_tag>::make(this->environment(), std::monostate{});
		}

		auto operator()(null_tag /*tag*/, visit_holder /*visit*/, const auto& /*null*/) const -> napi::value<null_tag> {
			return napi::value<null_tag>::make(this->environment(), nullptr);
		}

		// boolean
		auto operator()(boolean_tag /*tag*/, visit_holder /*visit*/, auto&& value) const -> napi::value<boolean_tag> {
			return napi::value<boolean_tag>::make(this->environment(), std::forward<decltype(value)>(value));
		}

		// number
		auto operator()(number_tag /*tag*/, visit_holder visit, auto&& value) const -> napi::value<number_tag> {
			return (*this)(number_tag_of<double>{}, visit, std::forward<decltype(value)>(value));
		}

		template <class Numeric>
		auto operator()(number_tag_of<Numeric> /*tag*/, visit_holder /*visit*/, auto&& value) const -> napi::value<number_tag> {
			return napi::value<number_tag>::make(this->environment(), Numeric{std::forward<decltype(value)>(value)});
		}

		// bigint
		auto operator()(bigint_tag /*tag*/, visit_holder visit, auto&& value) const -> js::referenceable_value<napi::value<bigint_tag>> {
			return (*this)(bigint_tag_of<bigint>{}, visit, std::forward<decltype(value)>(value));
		}

		auto operator()(bigint_tag_of<bigint> /*tag*/, visit_holder /*visit*/, auto&& value) const -> js::referenceable_value<napi::value<bigint_tag>> {
			return js::referenceable_value{napi::value<bigint_tag>::make(this->environment(), bigint{std::forward<decltype(value)>(value)})};
		}

		auto operator()(bigint_tag_of<bigint> /*tag*/, visit_holder /*visit*/, const bigint& value) const -> js::referenceable_value<napi::value<bigint_tag>> {
			return js::referenceable_value{napi::value<bigint_tag>::make(this->environment(), value)};
		}

		template <class Numeric>
		auto operator()(bigint_tag_of<Numeric> /*tag*/, visit_holder /*visit*/, auto&& value) const -> js::referenceable_value<napi::value<bigint_tag>> {
			return js::referenceable_value{napi::value<bigint_tag>::make(this->environment(), Numeric{std::forward<decltype(value)>(value)})};
		}

		// string
		auto operator()(string_tag /*tag*/, visit_holder visit, auto&& value) const -> js::referenceable_value<napi::value<string_tag>> {
			return (*this)(string_tag_of<char16_t>{}, visit, std::forward<decltype(value)>(value));
		}

		template <class Char>
		auto operator()(string_tag_of<Char> /*tag*/, visit_holder /*visit*/, auto&& value) const -> js::referenceable_value<napi::value<string_tag>> {
			return js::referenceable_value{napi::value<string_tag>::make(this->environment(), std::basic_string_view<Char>{std::forward<decltype(value)>(value)})};
		}

		// date
		auto operator()(date_tag /*tag*/, visit_holder /*visit*/, js_clock::time_point value) const -> js::referenceable_value<napi::value<date_tag>> {
			return js::referenceable_value{napi::value<date_tag>::make(this->environment(), std::forward<decltype(value)>(value))};
		}

		// vectors
		auto operator()(this auto& self, vector_tag /*tag*/, const auto& visit, auto&& list)
			-> js::deferred_receiver<napi::value<vector_tag>, decltype(self), decltype(visit), decltype(list)> {
			return {
				napi::value<vector_tag>::from(napi::invoke(napi_create_array_with_length, napi_env{self}, list.size())),
				[](napi::value<vector_tag> array, auto& self, const auto& visit, auto /*&&*/ list) -> void {
					int ii = 0;
					for (auto&& value : std::forward<decltype(list)>(list)) {
						auto* element = napi_value{visit(std::forward<decltype(value)>(value), self)};
						napi::invoke0(napi_set_element, napi_env{self}, napi_value{array}, ii++, element);
					}
				},
			};
		}

		template <std::size_t Size>
		auto operator()(this auto& self, tuple_tag<Size> /*tag*/, const auto& visit, auto&& tuple)
			-> js::deferred_receiver<napi::value<vector_tag>, decltype(self), decltype(visit), decltype(tuple)> {
			return {
				napi::value<vector_tag>::from(napi::invoke(napi_create_array_with_length, napi_env{self}, Size)),
				[](napi::value<vector_tag> array, auto& self, const auto& visit, auto /*&&*/ tuple) -> void {
					const auto [... indices ] = util::sequence<Size>;
					(..., [ & ]() {
						// nb: This is forwarded to *each* visitor. The visitor should be aware and only lvalue
						// reference members one at a time.
						auto* element = napi_value{visit(indices, std::forward<decltype(tuple)>(tuple), self)};
						napi::invoke0(napi_set_element, napi_env{self}, napi_value{array}, indices, element);
					}());
				}
			};
		}

		// arrays & dictionaries
		auto operator()(this auto& self, list_tag /*tag*/, const auto& visit, auto&& list)
			-> js::deferred_receiver<napi::value<list_tag>, decltype(self), decltype(visit), decltype(list)> {
			return {
				napi::value<list_tag>::from(napi::invoke(napi_create_array, napi_env{self})),
				[](napi::value<list_tag> array, auto& self, const auto& visit, auto /*&&*/ list) -> void {
					std::vector<napi_property_descriptor> properties;
					properties.reserve(std::size(list));
					auto accept_name = accept_napi_property_name{self, self};
					for (auto&& [ key, value ] : std::forward<decltype(list)>(list)) {
						properties.emplace_back(napi_property_descriptor{
							.utf8name{},
							.name = napi_value{visit.first(std::forward<decltype(key)>(key), accept_name)},

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

		auto operator()(this auto& self, dictionary_tag /*tag*/, const auto& visit, auto&& dictionary)
			-> js::deferred_receiver<napi::value<dictionary_tag>, decltype(self), decltype(visit), decltype(dictionary)> {
			return {
				napi::value<dictionary_tag>::from(napi::invoke(napi_create_object, napi_env{self})),
				[](napi::value<dictionary_tag> object, auto& self, const auto& visit, auto /*&&*/ dictionary) -> void {
					std::vector<napi_property_descriptor> properties;
					auto&& range = util::into_range(std::forward<decltype(dictionary)>(dictionary));
					properties.reserve(std::size(range));
					auto accept_name = accept_napi_property_name{self, self};
					for (auto&& [ key, value ] : std::forward<decltype(range)>(range)) {
						properties.emplace_back(napi_property_descriptor{
							.utf8name{},
							.name = napi_value{visit.first(std::forward<decltype(key)>(key), accept_name)},

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
		auto operator()(this auto& self, struct_tag<Size> /*tag*/, const auto& visit, auto&& dictionary)
			-> js::deferred_receiver<napi::value<dictionary_tag>, decltype(self), decltype(visit), decltype(dictionary)> {
			return {
				napi::value<dictionary_tag>::from(napi::invoke(napi_create_object, napi_env{self})),
				[](napi::value<dictionary_tag> object, auto& self, const auto& visit, auto /*&&*/ dictionary) -> void {
					std::array<napi_property_descriptor, Size> properties;
					const auto [... indices ] = util::sequence<Size>;
					(..., [ & ]() {
						const auto& visit_n = std::get<indices>(visit);
						properties[ indices ] = {
							.utf8name{},
							.name = napi_value{visit_n.first.get_local(self)},

							.method{},
							.getter{},
							.setter{},
							// nb: This is forwarded to *each* visitor. The visitor should be aware and only lvalue
							// reference members one at a time.
							.value = napi_value{visit_n.second(std::forward<decltype(dictionary)>(dictionary), self)},

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
template <class Meta>
struct accept<Meta, napi_value> : accept_napi_value<Meta, typename Meta::accept_context_type> {
		using accept_type = accept_napi_value<Meta, typename Meta::accept_context_type>;
		using accept_type::accept_type;
};

// Object key lookup via napi
template <class Meta, util::string_literal Key, class Type>
struct accept_property_value<Meta, Key, Type, napi_value> {
	public:
		explicit constexpr accept_property_value(auto* transfer) :
				second{transfer} {}

		auto operator()(dictionary_tag /*tag*/, const auto& visit, const auto& object) -> Type {
			if (auto local = first.get_local(visit.first); object.has(local)) {
				return visit.second(object.get(local), second);
			} else {
				return second(undefined_in_tag{}, visit.second, std::monostate{});
			}
		}

	private:
		visit_key_literal<Key, napi_value> first;
		accept_next<Meta, Type> second;
};

} // namespace js
