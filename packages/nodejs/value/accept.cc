module;
#include <cstring>
#include <iterator>
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

template <class Meta>
struct accept<Meta, Napi::Value> {
		accept() = delete;
		explicit accept(Napi::Env env) :
				env{env} {}

		auto operator()(undefined_tag /*tag*/, auto&& /*undefined*/) const -> Napi::Value {
			return env.Undefined();
		}

		auto operator()(null_tag /*tag*/, auto&& /*null*/) const -> Napi::Value {
			return env.Null();
		}

		auto operator()(boolean_tag /*tag*/, auto&& value) const -> Napi::Value {
			return Napi::Boolean::New(env, std::forward<decltype(value)>(value));
		}

		template <class Numeric>
		auto operator()(number_tag_of<Numeric> /*tag*/, auto&& value) const -> Napi::Value {
			return Napi::Number::New(env, std::forward<decltype(value)>(value));
		}

		auto operator()(string_tag /*tag*/, auto&& value) const -> Napi::Value {
			return Napi::String::New(env, std::forward<decltype(value)>(value));
		}

		auto operator()(date_tag /*tag*/, js_clock::time_point value) const -> Napi::Value {
			return Napi::Date::New(env, value.time_since_epoch().count());
		}

		auto operator()(list_tag /*tag*/, auto&& list) const -> Napi::Value {
			auto array = Napi::Array::New(env);
			for (auto&& [ key, value ] : list) {
				array.Set(
					invoke_visit(std::forward<decltype(key)>(key), *this),
					invoke_visit(std::forward<decltype(value)>(value), *this)
				);
			}
			return array;
		}

		auto operator()(dictionary_tag /*tag*/, auto&& dictionary) const -> Napi::Value {
			auto object = Napi::Object::New(env);
			for (auto&& [ key, value ] : dictionary) {
				object.Set(
					invoke_visit(std::forward<decltype(key)>(key), *this),
					invoke_visit(std::forward<decltype(value)>(value), *this)
				);
			}
			return object;
		}

		Napi::Env env;
};

} // namespace ivm::value
