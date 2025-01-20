module;
#include <utility>
#include <variant>
export module napi_js.visit;
import isolated_js;
import ivm.utility;
import napi_js.lock;
import napi_js.object;
import napi_js.utility;
import napi_js.value;
import nodejs;
import v8_js;
import v8;

namespace js {

// Delegate napi_value to various visitors
template <>
struct visit<void, napi_value>
		: napi_isolate_witness_lock,
			visit<void, v8::Local<v8::Boolean>>,
			visit<void, v8::Local<v8::Number>>,
			visit<void, v8::Local<v8::BigInt>>,
			visit<void, v8::Local<v8::String>>,
			visit<void, v8::Local<v8::Date>>,
			visit<void, v8::Local<v8::External>> {
	public:
		using visit<void, v8::Local<v8::Boolean>>::operator();
		using visit<void, v8::Local<v8::Number>>::operator();
		using visit<void, v8::Local<v8::BigInt>>::operator();
		using visit<void, v8::Local<v8::String>>::operator();
		using visit<void, v8::Local<v8::Date>>::operator();
		using visit<void, v8::Local<v8::External>>::operator();

		visit(napi_env env, v8::Isolate* isolate) :
				napi_isolate_witness_lock{env, isolate},
				visit<void, v8::Local<v8::String>>{*util::disambiguate_pointer<napi_isolate_witness_lock>(this)} {}
		// nb: TODO: Remove (again)
		explicit visit(napi_env env) :
				visit{env, v8::Isolate::GetCurrent()} {}

		auto operator()(napi_value value, const auto_accept auto& accept) const -> decltype(auto) {
			switch (js::napi::invoke(napi_typeof, env(), value)) {
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
						if (js::napi::invoke(napi_is_array, env(), value)) {
							return accept(list_tag{}, js::napi::object{env(), js::napi::value<js::list_tag>::from(value)}, visit_entry);
						} else if (js::napi::invoke(napi_is_date, env(), value)) {
							return (*this)(js::napi::to_v8(value).As<v8::Date>(), accept);
						} else if (js::napi::invoke(napi_is_promise, env(), value)) {
							return accept(promise_tag{}, value);
						}
						return accept(dictionary_tag{}, js::napi::object{env(), js::napi::value<js::object_tag>::from(value)}, visit_entry);
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
};

// Object key maker via napi
template <class Meta, util::string_literal Key>
struct visit_key_literal<Meta, Key, napi_value> : util::non_moveable {
	public:
		visit_key_literal() = default;
		explicit visit_key_literal(auto_heritage auto visit_heritage) :
				root{&visit_heritage.visit} {}

		auto operator()(const napi_witness_lock& lock, const auto& accept) const -> decltype(auto) {
			if (local_key == napi_value{}) {
				local_key = js::napi::value<string_tag>::make(lock.env(), Key.data());
			}
			return accept(string_tag{}, local_key);
		}

	private:
		const visit_root<Meta>* root;
		mutable napi_value local_key{};
};

} // namespace js
