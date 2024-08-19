module;
#include <cstring>
#include <functional>
#include <utility>
export module ivm.node:visit;
import :arguments;
import :environment;
import ivm.v8;
import ivm.value;
import napi;
import v8;

namespace ivm::value {

// Delegate Napi::Value to iv8 common acceptor
template <class Meta>
struct accept<Meta, Napi::Value> : accept<Meta, v8::Local<v8::Value>> {
		explicit accept(Napi::Env env) :
				accept<Meta, v8::Local<v8::Value>>{std::invoke([ &env ]() -> decltype(auto) {
					auto& ienv = environment::get(env);
					return accept<Meta, v8::Local<v8::Value>>{ienv.isolate(), ienv.context()};
				})},
				env_{env} {}

		auto operator()(v8::Local<v8::Value> value) const -> Napi::Value {
			napi_value holder{};
			std::memcpy(&holder, &value, sizeof(value));
			return {env_, holder};
		}

		auto operator()(auto_tag auto tag, auto&& value) -> Napi::Value {
			auto local = accept<Meta, v8::Local<v8::Value>>::operator()(tag, std::forward<decltype(value)>(value));
			return (*this)(local);
		}

		Napi::Env env_;
};

// Napi function arguments to list
template <>
struct visit<Napi::CallbackInfo> {
		auto operator()(const Napi::CallbackInfo& info, auto accept) -> decltype(auto) {
			return accept(vector_tag{}, ivm::napi::arguments{info});
		}
};

// Delegate Napi::Value to iv8 common visitor
template <>
struct visit<Napi::Value> {
		auto operator()(Napi::Value value, auto&& accept) -> decltype(auto) {
			v8::Local<v8::Value> local{};
			napi_value holder{value};
			std::memcpy(&local, &holder, sizeof(local));
			auto& ienv = environment::get(value.Env());
			auto* isolate = ienv.isolate();
			auto context = ienv.context();
			return invoke_visit(
				iv8::handle{local, {isolate, context}},
				std::forward<decltype(accept)>(accept)
			);
		}
};

} // namespace ivm::value
