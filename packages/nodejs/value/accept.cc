module;
#include <concepts>
#include <cstring>
#include <functional>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#include <version>
export module ivm.node:accept;
import :environment;
import ivm.value;
import ivm.utility;
import napi;
import v8;

namespace ivm {

export template <class Type>
auto v8_to_napi(const v8::Local<Type>& local) -> napi_value {
	napi_value value{};
	static_assert(std::is_pointer_v<napi_value>);
	static_assert(sizeof(void*) == sizeof(local));
	std::memcpy(&value, &local, sizeof(local));
	return value;
}

} // namespace ivm

namespace ivm::value {

template <util::string_literal Key>
struct napi_key_holder {
	public:
		auto get_local(Napi::Env env) const -> Napi::Value {
			if (local_key.IsEmpty()) {
				local_key = Napi::String::New(env, std::string_view{Key}.data(), Key.length());
			}
			return local_key;
		}

	private:
		mutable Napi::String local_key;
};

// Object key lookup via Napi
template <class Meta, util::string_literal Key>
struct visit_key<Meta, Key, Napi::Value> : napi_key_holder<Key> {
	public:
		constexpr visit_key(const auto_visit auto& visit) :
				root{visit} {}

		auto operator()(auto&& object, const auto& visit, const auto_accept auto& accept) const {
			auto local = this->get_local(root.env());
			using accepted_type = std::decay_t<decltype(visit.second(object.get(local), accept))>;
			if (object.has(local)) {
				return std::optional<accepted_type>{visit.second(object.get(local), accept)};
			} else {
				return std::optional<accepted_type>{};
			}
		}

	private:
		const visit_subject_t<Meta>& root;
};

// Object key maker via Napi
template <util::string_literal Key>
struct accept_key<Napi::Value, Key> : napi_key_holder<Key> {
		auto operator()(const auto& visit) const {
			return this->get_local(visit.env());
		}
};

// Fake acceptor for primitive values
template <>
struct accept<void, Napi::Env> {
	public:
		accept() = delete;
		explicit accept(Napi::Env env) :
				env_{env} {}

		auto env() const -> Napi::Env {
			return env_;
		}

		auto operator()(undefined_tag /*tag*/, auto&& /*undefined*/) const -> Napi::Value {
			return env_.Undefined();
		}

		auto operator()(null_tag /*tag*/, auto&& /*null*/) const -> Napi::Value {
			return env_.Null();
		}

		auto operator()(boolean_tag /*tag*/, auto&& value) const -> Napi::Boolean {
			return Napi::Boolean::New(env_, std::forward<decltype(value)>(value));
		}

		template <class Numeric>
		auto operator()(number_tag_of<Numeric> /*tag*/, auto&& value) const -> Napi::Number {
			return Napi::Number::New(env_, std::forward<decltype(value)>(value));
		}

		auto operator()(string_tag /*tag*/, auto&& value) const -> Napi::String {
			return Napi::String::New(env_, std::forward<decltype(value)>(value));
		}

		auto operator()(date_tag /*tag*/, js_clock::time_point value) const -> Napi::Date {
			return Napi::Date::New(env_, value.time_since_epoch().count());
		}

	private:
		Napi::Env env_;
};

// Explicit string acceptor
template <>
struct accept<void, Napi::String> : accept<void, Napi::Env> {
		using accept<void, Napi::Env>::accept;

		auto operator()(string_tag tag, auto&& value) const -> Napi::String {
			const accept<void, Napi::Env>& accept = *this;
			return accept(tag, std::forward<decltype(value)>(value));
		}
};

// Generic acceptor for most values
template <class Meta>
struct accept<Meta, Napi::Value> : accept<Meta, Napi::Env> {
	private:
		using accept_type = accept<Meta, Napi::Env>;

	public:
		using accept<Meta, Napi::Env>::accept;

		auto operator()(auto_tag auto tag, auto&& value) const -> Napi::Value
			requires std::invocable<accept_type, decltype(tag), decltype(value)> {
			const accept_type& accept = *this;
			return accept(tag, std::forward<decltype(value)>(value));
		}

		auto operator()(vector_tag /*tag*/, auto&& list, const auto& visit) const -> Napi::Value {
			auto array = Napi::Array::New(this->env());
			int ii = 0;
			for (auto&& value : list) {
				array.Set(Napi::Number::New(this->env(), ii++), visit(value, *this));
			}
			return array;
		}

		auto operator()(list_tag /*tag*/, auto&& list, const auto& visit) const -> Napi::Value {
			auto array = Napi::Array::New(this->env());
			for (auto&& [ key, value ] : list) {
				array.Set(
					visit.first(std::forward<decltype(key)>(key), *this),
					visit.second(std::forward<decltype(value)>(value), *this)
				);
			}
			return array;
		}

		auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> Napi::Value {
			auto object = Napi::Object::New(this->env());
			for (auto&& [ key, value ] : util::into_range(dictionary)) {
				object.Set(
					visit.first(std::forward<decltype(key)>(key), *this),
					visit.second(std::forward<decltype(value)>(value), *this)
				);
			}
			return object;
		}

		template <std::size_t Size>
		auto operator()(struct_tag<Size> /*tag*/, const auto& dictionary, const auto& visit) const -> Napi::Value {
			auto object = Napi::Object::New(this->env());
			std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					(invoke(std::integral_constant<size_t, Index>{}), ...);
				},
				[ & ]<std::size_t Index>(std::integral_constant<size_t, Index> index) {
					object.Set(
						visit.first(index, *this),
						visit.second(index, dictionary, *this)
					);
				},
				std::make_index_sequence<Size>{}
			);
			return object;
		}
};

} // namespace ivm::value
