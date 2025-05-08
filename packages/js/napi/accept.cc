module;
#include <concepts>
#include <functional>
#include <string_view>
#include <utility>
#include <variant>
export module napi_js.accept;
import isolated_js;
import ivm.utility;
import napi_js.lock;
import napi_js.primitive;
import napi_js.utility;
import napi_js.value;
import nodejs;
import v8;

namespace js {
using namespace napi;

// Fake acceptor for primitive values
template <>
struct accept<void, napi_env> : public napi_witness_lock {
	public:
		using napi_witness_lock::napi_witness_lock;

		auto operator()(undefined_tag /*tag*/, const auto& /*undefined*/) const -> napi_value {
			return js::napi::value<undefined_tag>::make(env(), std::monostate{});
		}

		auto operator()(null_tag /*tag*/, const auto& /*null*/) const -> napi_value {
			return js::napi::value<null_tag>::make(env(), nullptr);
		}

		auto operator()(boolean_tag /*tag*/, auto&& value) const -> napi_value {
			return js::napi::invoke(napi_get_boolean, env(), std::forward<decltype(value)>(value));
		}

		auto operator()(number_tag /*tag*/, auto&& value) const -> napi_value {
			return js::napi::value<number_tag>::make(env(), double{std::forward<decltype(value)>(value)});
		}

		template <class Numeric>
		auto operator()(number_tag_of<Numeric> /*tag*/, auto&& value) const -> napi_value {
			return js::napi::value<number_tag_of<Numeric>>::make(env(), std::forward<decltype(value)>(value));
		}

		auto operator()(bigint_tag /*tag*/, const bigint& value) const -> napi_value {
			return js::napi::value<bigint_tag>::make(env(), value);
		}

		auto operator()(bigint_tag /*tag*/, auto&& value) const -> napi_value {
			return js::napi::value<bigint_tag>::make(env(), bigint{std::forward<decltype(value)>(value)});
		}

		template <class Numeric>
		auto operator()(bigint_tag_of<Numeric> /*tag*/, auto&& value) const -> napi_value {
			return js::napi::value<bigint_tag_of<Numeric>>::make(env(), std::forward<decltype(value)>(value));
		}

		auto operator()(string_tag /*tag*/, auto&& value) const -> napi_value {
			return js::napi::value<string_tag>::make(env(), std::u16string_view{std::forward<decltype(value)>(value)});
		}

		auto operator()(string_tag_of<char> /*tag*/, auto&& value) const -> napi_value {
			return js::napi::value<string_tag_of<char>>::make(env(), std::forward<decltype(value)>(value));
		}

		auto operator()(date_tag /*tag*/, js_clock::time_point value) const -> napi_value {
			return js::napi::invoke(napi_create_date, env(), value.time_since_epoch().count());
		}
};

// Generic acceptor for most values
template <class Meta>
struct accept<Meta, napi_value> : accept<Meta, napi_env> {
	private:
		using accept_type = accept<Meta, napi_env>;

	public:
		using accept<Meta, napi_env>::accept;

		auto operator()(auto_tag auto tag, auto&& value) const -> napi_value
			requires std::invocable<accept_type, decltype(tag), decltype(value)> {
			return accept_type::operator()(tag, std::forward<decltype(value)>(value));
		}

		auto operator()(vector_tag /*tag*/, auto&& list, const auto& visit) const -> napi_value {
			auto array = js::napi::invoke(napi_create_array_with_length, this->env(), list.size());
			int ii = 0;
			for (auto&& value : std::forward<decltype(list)>(list)) {
				js::napi::invoke0(
					napi_set_element,
					this->env(),
					array,
					ii++,
					visit(std::forward<decltype(value)>(value), *this)
				);
			}
			return array;
		}

		auto operator()(list_tag /*tag*/, auto&& list, const auto& visit) const -> napi_value {
			auto array = js::napi::invoke(napi_create_array, this->env());
			for (auto&& [ key, value ] : std::forward<decltype(list)>(list)) {
				js::napi::invoke0(
					napi_set_property,
					this->env(),
					array,
					visit.first(std::forward<decltype(key)>(key), *this),
					visit.second(std::forward<decltype(value)>(value), *this)
				);
			}
			return array;
		}

		auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> napi_value {
			auto object = js::napi::invoke(napi_create_object, this->env());
			for (auto&& [ key, value ] : util::into_range(std::forward<decltype(dictionary)>(dictionary))) {
				js::napi::invoke0(
					napi_set_property,
					this->env(),
					object,
					visit.first(std::forward<decltype(key)>(key), *this),
					visit.second(std::forward<decltype(value)>(value), *this)
				);
			}
			return object;
		}

		template <std::size_t Size>
		auto operator()(struct_tag<Size> /*tag*/, auto&& dictionary, const auto& visit) const -> napi_value {
			auto object = js::napi::invoke(napi_create_object, this->env());
			std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					(invoke(std::integral_constant<size_t, Index>{}), ...);
				},
				[ & ]<std::size_t Index>(std::integral_constant<size_t, Index> /*index*/) {
					const auto& visit_n = std::get<Index>(visit);
					js::napi::invoke0(
						napi_set_property,
						this->env(),
						object,
						visit_n.first.get(),
						// nb: This is forwarded to *each* visitor. The visitor should be aware and only lvalue
						// reference members one at a time.
						visit_n.second(std::forward<decltype(dictionary)>(dictionary), *this)
					);
				},
				std::make_index_sequence<Size>{}
			);
			return object;
		}

		template <std::size_t Size>
		auto operator()(tuple_tag<Size> /*tag*/, auto tuple, const auto& visit) const -> napi_value {
			auto array = js::napi::invoke(napi_create_array_with_length, this->env(), Size);
			std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					(invoke(std::integral_constant<size_t, Index>{}), ...);
				},
				[ & ]<std::size_t Index>(std::integral_constant<size_t, Index> index) {
					js::napi::invoke0(
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
};

// Object key lookup via napi
template <class Meta, util::string_literal Key, class Type>
struct accept_property_value<Meta, Key, Type, napi_value> {
	public:
		explicit constexpr accept_property_value(auto_heritage auto accept_heritage) :
				visit_key_{key_literal_, accept_heritage.visit},
				accept_value_{accept_heritage} {}

		auto operator()(dictionary_tag /*tag*/, const auto& object, const auto& visit) const {
			if (auto local = visit_key_.get(); object.has(local)) {
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
struct accept<void, js::napi::value<Tag>> {
		auto operator()(undefined_tag /*tag*/, std::monostate /*undefined*/) const -> js::napi::value<Tag> {
			return js::napi::value<Tag>::from(nullptr);
		}

		auto operator()(Tag /*tag*/, napi_value value) const -> js::napi::value<Tag> {
			return js::napi::value<Tag>::from(std::forward<decltype(value)>(value));
		}

		template <class Type>
		auto operator()(Tag /*tag*/, v8::Local<Type> value) const -> js::napi::value<Tag> {
			return js::napi::value<Tag>::from(from_v8(value));
		}
};

} // namespace js
