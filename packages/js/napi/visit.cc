module;
#include <utility>
#include <variant>
export module ivm.napi:visit;
import :object;
import :utility;
import :value;
import ivm.iv8;
import ivm.js;
import ivm.utility;
import napi;
import v8;

namespace ivm::js {

// Delegate napi_value to various visitors
template <>
struct visit<void, napi_value> : visit<void, v8::Local<v8::Value>> {
	public:
		using visit<void, v8::Local<v8::Value>>::operator();

		visit(napi_env env, v8::Isolate* isolate, v8::Local<v8::Context> context) :
				visit<void, v8::Local<v8::Value>>{isolate, context},
				env_{env} {}
		visit(napi_env env, v8::Isolate* isolate) :
				visit{env, isolate, isolate->GetCurrentContext()} {}
		// nb: TODO: Remove (again)
		explicit visit(napi_env env) :
				visit{env, v8::Isolate::GetCurrent()} {}

		[[nodiscard]] auto env() const -> napi_env {
			return env_;
		}

		auto operator()(napi_value value, const auto_accept auto& accept) const -> decltype(auto) {
			switch (js::napi::invoke(napi_typeof, env_, value)) {
				case napi_boolean:
					return (*this)(js::napi::to_v8(value).As<v8::Boolean>(), accept);
				case napi_number:
					return (*this)(js::napi::to_v8(value).As<v8::Number>(), accept);
				case napi_bigint:
					return (*this)(js::napi::to_v8(value).As<v8::BigInt>(), accept);
				case napi_string:
					return (*this)(js::napi::to_v8(value).As<v8::String>(), accept);
				case napi_object:
					{
						auto visit_entry = std::pair<const visit&, const visit&>{*this, *this};
						if (js::napi::invoke(napi_is_array, env_, value)) {
							return accept(list_tag{}, js::napi::object{env_, js::napi::value<js::list_tag>::from(value)}, visit_entry);
						} else if (js::napi::invoke(napi_is_date, env_, value)) {
							return (*this)(js::napi::to_v8(value).As<v8::Date>(), accept);
						} else if (js::napi::invoke(napi_is_promise, env_, value)) {
							return accept(promise_tag{}, value);
						}
						return accept(dictionary_tag{}, js::napi::object{env_, js::napi::value<js::object_tag>::from(value)}, visit_entry);
					}
				case napi_external:
					return (*this)(js::napi::to_v8(value).As<v8::External>(), accept);
				case napi_symbol:
					return accept(symbol_tag{}, value);
				case napi_null:
					return accept(null_tag{}, value);
				case napi_undefined:
					return accept(undefined_tag{}, std::monostate{});
				case napi_function:
					return accept(function_tag{}, value);
			}
		}

	private:
		napi_env env_;
};

// Object key maker via napi
template <util::string_literal Key>
struct visit<void, key_for<Key, napi_value>> {
	public:
		auto operator()(const auto& context, const auto& accept) const -> decltype(auto) {
			if (local_key == napi_value{}) {
				local_key = js::napi::value<string_tag>::make(context.env(), Key.data());
			}
			return accept(string_tag{}, local_key);
		}

	private:
		mutable napi_value local_key{};
};

} // namespace ivm::js
