module;
#include <functional>
#include <string_view>
#include <utility>
#include <variant>
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

template <class Environment, class Type>
struct accept_napi_value;

// Helper which accepts property names and uses `make_property_name` to make an internalized property name
template <class Environment>
struct accept_napi_property_name : napi::environment_scope<Environment> {
		explicit accept_napi_property_name(const auto& accept) :
				napi::environment_scope<Environment>{accept} {}

		template <auto_tag Tag>
		auto operator()(Tag /*tag*/, auto&& value) const -> napi::value<primitive_tag> {
			return js::napi::value<Tag>::make_property_name(this->environment(), std::forward<decltype(value)>(value));
		}
};

template <class Environment, class Type>
accept_napi_property_name(const accept_napi_value<Environment, Type>&) -> accept_napi_property_name<Environment>;

// Non-recursive primitive / intrinsic acceptor which returns varying `value<T>` types depending on the subject
template <class Environment, class Type>
struct accept_napi_value : napi::environment_scope<Environment> {
	public:
		explicit accept_napi_value(auto* /*previous*/, auto& env) :
				napi::environment_scope<Environment>{env} {}

		// undefined & null
		auto operator()(undefined_tag /*tag*/, const auto& /*undefined*/) const -> Type {
			return napi::value<undefined_tag>::make(this->environment(), std::monostate{});
		}

		auto operator()(null_tag /*tag*/, const auto& /*null*/) const -> Type {
			return napi::value<null_tag>::make(this->environment(), nullptr);
		}

		// boolean
		auto operator()(boolean_tag /*tag*/, auto&& value) const -> Type {
			return napi::value<boolean_tag>::make(this->environment(), std::forward<decltype(value)>(value));
		}

		// number
		auto operator()(number_tag /*tag*/, auto&& value) const -> Type {
			return js::napi::value<number_tag>::make(this->environment(), double{std::forward<decltype(value)>(value)});
		}

		template <class Numeric>
		auto operator()(number_tag_of<Numeric> /*tag*/, auto&& value) const -> Type {
			return napi::value<number_tag_of<Numeric>>::make(this->environment(), std::forward<decltype(value)>(value));
		}

		// bigint
		auto operator()(bigint_tag /*tag*/, const bigint& value) const -> Type {
			return js::napi::value<bigint_tag>::make(this->environment(), value);
		}

		auto operator()(bigint_tag /*tag*/, auto&& value) const -> Type {
			return js::napi::value<bigint_tag>::make(this->environment(), bigint{std::forward<decltype(value)>(value)});
		}

		template <class Numeric>
		auto operator()(bigint_tag_of<Numeric> /*tag*/, auto&& value) const -> Type {
			return napi::value<bigint_tag_of<Numeric>>::make(this->environment(), std::forward<decltype(value)>(value));
		}

		// string
		auto operator()(string_tag /*tag*/, auto&& value) const -> Type {
			return js::napi::value<string_tag>::make(this->environment(), std::u16string_view{std::forward<decltype(value)>(value)});
		}

		template <class Char>
		auto operator()(string_tag_of<Char> /*tag*/, auto&& value) const -> Type {
			return napi::value<string_tag_of<Char>>::make(this->environment(), std::forward<decltype(value)>(value));
		}

		// date
		auto operator()(date_tag /*tag*/, js_clock::time_point value) const -> Type {
			return napi::value<date_tag>::make(this->environment(), std::forward<decltype(value)>(value));
		}

		// vectors
		auto operator()(vector_tag /*tag*/, auto&& list, const auto& visit) const -> Type {
			auto array = value<vector_tag>::from(napi::invoke(napi_create_array_with_length, napi_env{*this}, list.size()));
			int ii = 0;
			for (auto&& value : std::forward<decltype(list)>(list)) {
				auto* element = napi_value{visit(std::forward<decltype(value)>(value), *this)};
				napi::invoke0(napi_set_element, napi_env{*this}, napi_value{array}, ii++, element);
			}
			return array;
		}

		template <std::size_t Size>
		auto operator()(tuple_tag<Size> /*tag*/, auto&& tuple, const auto& visit) const -> Type {
			auto array = value<vector_tag>::from(napi::invoke(napi_create_array_with_length, napi_env{*this}, Size));
			std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					(invoke(std::integral_constant<size_t, Index>{}), ...);
				},
				[ & ]<std::size_t Index>(std::integral_constant<size_t, Index> index) {
					// nb: This is forwarded to *each* visitor. The visitor should be aware and only lvalue
					// reference members one at a time.
					auto* element = napi_value{visit(index, std::forward<decltype(tuple)>(tuple), *this)};
					napi::invoke0(napi_set_element, napi_env{*this}, napi_value{array}, Index, element);
				},
				std::make_index_sequence<Size>{}
			);
			return array;
		}

		// arrays & dictionaries
		auto operator()(list_tag /*tag*/, auto&& list, const auto& visit) const -> Type {
			std::vector<napi_property_descriptor> properties;
			properties.reserve(std::size(list));
			for (auto&& [ key, value ] : std::forward<decltype(list)>(list)) {
				properties.emplace_back(napi_property_descriptor{
					.utf8name{},
					.name = napi_value{visit.first(std::forward<decltype(key)>(key), accept_napi_property_name{*this})},

					.method{},
					.getter{},
					.setter{},
					.value = napi_value{visit.second(std::forward<decltype(value)>(value), *this)},

					// NOLINTNEXTLINE(hicpp-signed-bitwise)
					.attributes = static_cast<napi_property_attributes>(napi_writable | napi_enumerable | napi_configurable),
					.data{},
				});
			}
			auto array = value<list_tag>::from(napi::invoke(napi_create_array, napi_env{*this}));
			napi::invoke0(napi_define_properties, napi_env{*this}, napi_value{array}, properties.size(), properties.data());
			return array;
		}

		auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> Type {
			std::vector<napi_property_descriptor> properties;
			auto&& range = util::into_range(std::forward<decltype(dictionary)>(dictionary));
			properties.reserve(std::size(range));
			for (auto&& [ key, value ] : std::forward<decltype(range)>(range)) {
				properties.emplace_back(napi_property_descriptor{
					.utf8name{},
					.name = napi_value{visit.first(std::forward<decltype(key)>(key), accept_napi_property_name{*this})},

					.method{},
					.getter{},
					.setter{},
					.value = napi_value{visit.second(std::forward<decltype(value)>(value), *this)},

					// NOLINTNEXTLINE(hicpp-signed-bitwise)
					.attributes = static_cast<napi_property_attributes>(napi_writable | napi_enumerable | napi_configurable),
					.data{},
				});
			}
			auto object = value<dictionary_tag>::from(napi::invoke(napi_create_object, napi_env{*this}));
			napi::invoke0(napi_define_properties, napi_env{*this}, napi_value{object}, properties.size(), properties.data());
			return object;
		}

		// structs
		template <std::size_t Size>
		auto operator()(struct_tag<Size> /*tag*/, auto&& dictionary, const auto& visit) const -> Type {
			std::array<napi_property_descriptor, Size> properties;
			std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					(invoke(std::integral_constant<size_t, Index>{}), ...);
				},
				[ & ]<std::size_t Index>(std::integral_constant<size_t, Index> /*index*/) {
					const auto& visit_n = std::get<Index>(visit);
					properties[ Index ] = {
						.utf8name{},
						.name = napi_value{visit_n.first.get_local(*this)},

						.method{},
						.getter{},
						.setter{},
						// nb: This is forwarded to *each* visitor. The visitor should be aware and only lvalue
						// reference members one at a time.
						.value = napi_value{visit_n.second(std::forward<decltype(dictionary)>(dictionary), *this)},

						// NOLINTNEXTLINE(hicpp-signed-bitwise)
						.attributes = static_cast<napi_property_attributes>(napi_writable | napi_enumerable | napi_configurable),
						.data{},
					};
				},
				std::make_index_sequence<Size>{}
			);
			auto object = value<dictionary_tag>::from(napi::invoke(napi_create_object, napi_env{*this}));
			napi::invoke0(napi_define_properties, napi_env{*this}, napi_value{object}, properties.size(), properties.data());
			return object;
		}
};

// Plain napi_value acceptor
template <class Meta>
struct accept<Meta, napi_value> : accept_napi_value<typename Meta::accept_context_type, napi_value> {
		using accept_napi_value<typename Meta::accept_context_type, napi_value>::accept_napi_value;
};

// Object key lookup via napi
template <class Meta, util::string_literal Key, class Type>
struct accept_property_value<Meta, Key, Type, napi_value> {
	public:
		explicit constexpr accept_property_value(auto* previous) :
				second{previous} {}

		auto operator()(dictionary_tag /*tag*/, const auto& object, const auto& visit) const {
			if (auto local = first.get_local(visit.first); object.has(local)) {
				return visit.second(object.get(local), second);
			} else {
				return second(undefined_in_tag{}, std::monostate{});
			}
		}

	private:
		visit_key_literal<Key, napi_value> first;
		accept_next<Meta, Type> second;
};

} // namespace js
