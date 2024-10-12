module;
#include <cstring>
#include <utility>
#include <variant>
export module ivm.node:visit;
import :accept;
import :arguments;
import :environment;
import :object;
import ivm.iv8;
import ivm.utility;
import ivm.value;
import napi;
import v8;

namespace ivm {

auto napi_to_v8(napi_value value) -> v8::Local<v8::Value> {
	v8::Local<v8::Value> local{};
	static_assert(std::is_pointer_v<napi_value>);
	static_assert(sizeof(void*) == sizeof(local));
	std::memcpy(&local, &value, sizeof(local));
	return local;
}

} // namespace ivm

namespace ivm::value {

// Delegate Napi::Value to various visitors
template <>
struct visit<Napi::Value> : visit<v8::Local<v8::Value>> {
		using visit<v8::Local<v8::Value>>::operator();

		visit(v8::Isolate* isolate, v8::Local<v8::Context> context) :
				visit<v8::Local<v8::Value>>{isolate, context} {}
		visit(v8::Isolate* isolate) :
				visit{isolate, isolate->GetCurrentContext()} {}
		// TODO: remove ASAP!
		visit() :
				visit{v8::Isolate::GetCurrent()} {}

		auto operator()(Napi::Value value, const auto& accept) const -> decltype(auto) {
			switch (value.Type()) {
				case napi_boolean:
					return (*this)(napi_to_v8(value).As<v8::Boolean>(), accept);
				case napi_number:
					return (*this)(napi_to_v8(value).As<v8::Number>(), accept);
				case napi_bigint:
					return (*this)(napi_to_v8(value).As<v8::BigInt>(), accept);
				case napi_string:
					return (*this)(napi_to_v8(value).As<v8::String>(), accept);
				case napi_object:
					if (value.IsArray()) {
						return accept(list_tag{}, ivm::napi::object{value.As<Napi::Object>()});
					} else if (value.IsDate()) {
						return (*this)(napi_to_v8(value).As<v8::Date>(), accept);
					} else if (value.IsPromise()) {
						return accept(promise_tag{}, value);
					}
					return accept(dictionary_tag{}, ivm::napi::object{value.As<Napi::Object>()});
				case napi_external:
					return (*this)(napi_to_v8(value).As<v8::External>(), accept);
				case napi_symbol:
					return accept(symbol_tag{}, value);
				case napi_null:
					return accept(null_tag{}, std::nullptr_t{});
				case napi_undefined:
					return accept(undefined_tag{}, std::monostate{});
				case napi_function:
					return accept(value_tag{}, std::type_identity<void>{});
			}
		}

		auto operator()(const Napi::CallbackInfo& info, const auto& accept) const -> decltype(auto) {
			return accept(vector_tag{}, ivm::napi::arguments{info});
		}
};

// Napi function arguments to list
template <>
struct visit<Napi::CallbackInfo> : visit<Napi::Value> {
		using visit<Napi::Value>::visit;
};

} // namespace ivm::value
