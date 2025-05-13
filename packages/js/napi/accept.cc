module;
#include <concepts>
#include <functional>
#include <string_view>
#include <utility>
#include <variant>
export module napi_js.accept;
import isolated_js;
import ivm.utility;
import napi_js.dictionary;
import napi_js.lock;
import napi_js.primitive;
import napi_js.utility;
import napi_js.value;
import nodejs;

namespace js {
using napi::value;

// Non-recursive primitive / intrinsic acceptor
struct accept_napi_intrinsics : public napi::napi_witness_lock {
	public:
		using napi_witness_lock::napi_witness_lock;

		// undefined & null
		auto operator()(undefined_tag /*tag*/, const auto& /*undefined*/) const {
			return napi::value<undefined_tag>::make(env(), std::monostate{});
		}

		auto operator()(null_tag /*tag*/, const auto& /*null*/) const {
			return napi::value<null_tag>::make(env(), nullptr);
		}

		// boolean
		auto operator()(boolean_tag /*tag*/, auto&& value) const {
			return napi::value<boolean_tag>::make(env(), std::forward<decltype(value)>(value));
		}

		// number
		auto operator()(number_tag /*tag*/, auto&& value) const {
			return js::napi::value<number_tag>::make(env(), double{std::forward<decltype(value)>(value)});
		}

		template <class Numeric>
		auto operator()(number_tag_of<Numeric> /*tag*/, auto&& value) const {
			return napi::value<number_tag_of<Numeric>>::make(env(), std::forward<decltype(value)>(value));
		}

		// bigint
		auto operator()(bigint_tag /*tag*/, const bigint& value) const {
			return js::napi::value<bigint_tag>::make(env(), value);
		}

		auto operator()(bigint_tag /*tag*/, auto&& value) const {
			return js::napi::value<bigint_tag>::make(env(), bigint{std::forward<decltype(value)>(value)});
		}

		template <class Numeric>
		auto operator()(bigint_tag_of<Numeric> /*tag*/, auto&& value) const {
			return napi::value<bigint_tag_of<Numeric>>::make(env(), std::forward<decltype(value)>(value));
		}

		// string
		auto operator()(string_tag /*tag*/, auto&& value) const {
			return js::napi::value<string_tag>::make(env(), std::u16string_view{std::forward<decltype(value)>(value)});
		}

		auto operator()(string_tag_of<char> /*tag*/, auto&& value) const {
			return napi::value<string_tag_of<char>>::make(env(), std::forward<decltype(value)>(value));
		}

		// date
		auto operator()(date_tag /*tag*/, js_clock::time_point value) const {
			return napi::value<date_tag>::make(env(), std::forward<decltype(value)>(value));
		}
};

// Recursive acceptor which can handle arrays & objects
template <>
struct accept<void, napi_value> : accept_napi_intrinsics {
	public:
		using accept_napi_intrinsics::accept_napi_intrinsics;

		// forward intrinsic value
		auto operator()(auto_tag auto tag, auto&&... args) const -> napi_value
			requires std::invocable<accept_napi_intrinsics, decltype(tag), decltype(args)...> {
			const accept_napi_intrinsics& accept = *this;
			return accept(tag, std::forward<decltype(args)>(args)...);
		}

		// vectors
		auto operator()(vector_tag /*tag*/, auto&& list, const auto& visit) const -> napi_value {
			auto array = value<vector_tag>::from(napi::invoke(napi_create_array_with_length, this->env(), list.size()));
			int ii = 0;
			for (auto&& value : std::forward<decltype(list)>(list)) {
				napi::invoke0(
					napi_set_element,
					this->env(),
					napi_value{array},
					ii++,
					visit(std::forward<decltype(value)>(value), *this)
				);
			}
			return array;
		}

		template <std::size_t Size>
		auto operator()(tuple_tag<Size> /*tag*/, auto tuple, const auto& visit) const -> napi_value {
			auto array = value<vector_tag>::from(napi::invoke(napi_create_array_with_length, this->env(), Size));
			std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					(invoke(std::integral_constant<size_t, Index>{}), ...);
				},
				[ & ]<std::size_t Index>(std::integral_constant<size_t, Index> index) {
					napi::invoke0(
						napi_set_element,
						this->env(),
						array,
						Index,
						// nb: This is forwarded to *each* visitor. The visitor should be aware and only lvalue
						// reference members one at a time.
						visit(index, tuple, *this)
					);
				},
				std::make_index_sequence<Size>{}
			);
			return array;
		}

		// arrays & dictionaries
		auto operator()(list_tag /*tag*/, auto&& list, const auto& visit) const -> napi_value {
			std::vector<napi_property_descriptor> properties;
			properties.reserve(std::size(list));
			for (auto&& [ key, value ] : std::forward<decltype(list)>(list)) {
				properties.emplace_back(napi_property_descriptor{
					.utf8name{},
					.name = visit.first(std::forward<decltype(key)>(key), *this),

					.method{},
					.getter{},
					.setter{},
					.value = visit.second(std::forward<decltype(value)>(value), *this),

					// NOLINTNEXTLINE(hicpp-signed-bitwise)
					.attributes = static_cast<napi_property_attributes>(napi_writable | napi_enumerable | napi_configurable),
					.data{},
				});
			}
			auto array = value<list_tag>::from(napi::invoke(napi_create_array, this->env()));
			napi::invoke0(napi_define_properties, this->env(), napi_value{array}, properties.size(), properties.data());
			return array;
		}

		auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> napi_value {
			std::vector<napi_property_descriptor> properties;
			auto&& range = util::into_range(std::forward<decltype(dictionary)>(dictionary));
			properties.reserve(std::size(range));
			for (auto&& [ key, value ] : std::forward<decltype(range)>(range)) {
				properties.emplace_back(napi_property_descriptor{
					.utf8name{},
					.name = visit.first(std::forward<decltype(key)>(key), *this),

					.method{},
					.getter{},
					.setter{},
					.value = visit.second(std::forward<decltype(value)>(value), *this),

					// NOLINTNEXTLINE(hicpp-signed-bitwise)
					.attributes = static_cast<napi_property_attributes>(napi_writable | napi_enumerable | napi_configurable),
					.data{},
				});
			}
			auto object = value<dictionary_tag>::from(napi::invoke(napi_create_object, this->env()));
			napi::invoke0(napi_define_properties, this->env(), napi_value{object}, properties.size(), properties.data());
			return object;
		}

		// structs
		template <std::size_t Size>
		auto operator()(struct_tag<Size> /*tag*/, auto&& dictionary, const auto& visit) const -> napi_value {
			std::array<napi_property_descriptor, Size> properties;
			std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					(invoke(std::integral_constant<size_t, Index>{}), ...);
				},
				[ & ]<std::size_t Index>(std::integral_constant<size_t, Index> /*index*/) {
					const auto& visit_n = std::get<Index>(visit);
					properties[ Index ] = {
						.utf8name = visit_n.first.get_utf8(),
						.name{},

						.method{},
						.getter{},
						.setter{},
						// nb: This is forwarded to *each* visitor. The visitor should be aware and only lvalue
						// reference members one at a time.
						.value = visit_n.second(std::forward<decltype(dictionary)>(dictionary), *this),

						// NOLINTNEXTLINE(hicpp-signed-bitwise)
						.attributes = static_cast<napi_property_attributes>(napi_writable | napi_enumerable | napi_configurable),
						.data{},
					};
				},
				std::make_index_sequence<Size>{}
			);
			auto object = value<dictionary_tag>::from(napi::invoke(napi_create_object, this->env()));
			napi::invoke0(napi_define_properties, this->env(), napi_value{object}, properties.size(), properties.data());
			return object;
		}
};

// Object key lookup via napi
template <class Meta, util::string_literal Key, class Type>
struct accept_property_value<Meta, Key, Type, napi_value> {
	public:
		explicit constexpr accept_property_value(auto_heritage auto accept_heritage) :
				visit_key_{key_literal_, accept_heritage.visit},
				accept_value_{accept_heritage} {}

		auto operator()(dictionary_tag /*tag*/, const auto& object, const auto& visit) const {
			if (auto local = visit_key_.get_local(); object.has(local)) {
				return visit.second(object.get(local), accept_value_);
			} else {
				return accept_value_(undefined_in_tag{}, std::monostate{});
			}
		}

	private:
		using key_literal_type = visit_key_literal<Key, napi_value>;
		key_literal_type key_literal_;
		key_literal_type::visit visit_key_;
		accept_next<Meta, Type> accept_value_;
};

// Tagged value acceptor
template <class Tag>
struct accept<void, napi::value<Tag>> {
		auto operator()(Tag /*tag*/, napi::bound_value<Tag> value) const -> napi::value<Tag> {
			return napi::value<Tag>{value};
		}

		auto operator()(Tag /*tag*/, napi::value<Tag> value) const -> napi::value<Tag> {
			return value;
		}
};

} // namespace js
