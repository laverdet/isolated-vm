module;
#include <functional>
#include <utility>
#include <variant>
export module napi_js.accept;
import isolated_js;
import ivm.utility;
import napi_js.utility;
import napi_js.value;
import nodejs;
import v8;

namespace js {

// Fake acceptor for primitive values
template <>
struct accept<void, napi_env> {
	public:
		accept() = delete;
		explicit accept(napi_env env) :
				env_{env} {}

		auto env() const -> napi_env {
			return env_;
		}

		auto operator()(undefined_tag /*tag*/, const auto& /*undefined*/) const -> napi_value {
			return js::napi::value<undefined_tag>::make(env_);
		}

		auto operator()(null_tag /*tag*/, const auto& /*null*/) const -> napi_value {
			return js::napi::value<null_tag>::make(env_);
		}

		auto operator()(boolean_tag /*tag*/, auto&& value) const -> napi_value {
			return js::napi::invoke(napi_get_boolean, env_, std::forward<decltype(value)>(value));
		}

		template <class Numeric>
		auto operator()(number_tag_of<Numeric> /*tag*/, auto&& value) const -> napi_value {
			return js::napi::value<number_tag_of<Numeric>>::make(env_, std::forward<decltype(value)>(value));
		}

		template <class Numeric>
		auto operator()(bigint_tag_of<Numeric> /*tag*/, auto&& value) const -> napi_value {
			return js::napi::value<bigint_tag_of<Numeric>>::make(env_, std::forward<decltype(value)>(value));
		}

		template <class Char>
		auto operator()(string_tag_of<Char> /*tag*/, auto&& value) const -> napi_value {
			return js::napi::value<string_tag_of<Char>>::make(env_, std::forward<decltype(value)>(value));
		}

		auto operator()(date_tag /*tag*/, js_clock::time_point value) const -> napi_value {
			return js::napi::invoke(napi_create_date, env_, value.time_since_epoch().count());
		}

	private:
		napi_env env_;
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
					accept<void, accept_immediate<string_tag>> accept_string;
					js::napi::invoke0(
						napi_set_property,
						this->env(),
						object,
						visit_n.first(*this, accept_string),
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
struct accept<Meta, value_by_key<Key, Type, napi_value>> {
	public:
		accept() = delete;
		explicit constexpr accept(const visit_root<Meta>& visit) :
				root{&visit},
				accept_value{visit} {}

		auto operator()(dictionary_tag /*tag*/, const auto& object, const auto& visit) const {
			accept<void, accept_immediate<string_tag>> accept_string;
			auto local = visit_key(*root, accept_string);
			if (object.has(local)) {
				return visit.second(object.get(local), accept_value);
			} else {
				return accept_value(undefined_in_tag{}, std::monostate{});
			}
		}

	private:
		const visit_root<Meta>* root;
		visit_key_literal<Key, napi_value> visit_key;
		accept_next<Meta, Type> accept_value;
};

// Tagged value acceptor
template <class Tag>
struct accept<void, js::napi::value<Tag>> : accept<void, void> {
		using accept<void, void>::accept;

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
