module;
#include <cstring>
#include <stdexcept>
#include <utility>
export module ivm.node:visit;
import :arguments;
import :environment;
import ivm.v8;
import ivm.value;
import ivm.utility;
import napi;
import v8;

namespace ivm {

thread_local napi_env delegated_env{};

// Return the given `napi_env` which corresponds to the `v8::Value` that is currently being visited.
export auto get_delegated_visit_napi_env() -> napi_env {
	if (delegated_env == napi_env{}) {
		throw std::runtime_error{"No delegated napi_env"};
	}
	return delegated_env;
}

// Marks an env belong to the current visit
auto delegated_visit_scope(napi_env env) {
	if (delegated_env != napi_env{}) {
		throw std::runtime_error{"Delegated env already set"};
	}
	delegated_env = env;
	return scope_exit{defaulter_finalizer{delegated_env}};
}

export template <class Type>
auto napi_to_v8(napi_value value) -> v8::Local<Type> {
	v8::Local<Type> local{};
	static_assert(std::is_pointer_v<napi_value>);
	static_assert(sizeof(void*) == sizeof(local));
	std::memcpy(&local, &value, sizeof(local));
	return local;
}

export template <class Type>
auto v8_to_napi(v8::Local<Type> local) -> napi_value {
	napi_value value{};
	static_assert(std::is_pointer_v<napi_value>);
	static_assert(sizeof(void*) == sizeof(local));
	std::memcpy(&value, &local, sizeof(local));
	return value;
}

} // namespace ivm

namespace ivm::napi {

// Memoize `environment&` to avoid calling back into napi for each value
export class napi_callback_info_memo : non_copyable {
	public:
		napi_callback_info_memo(napi_callback_info_memo&&) = delete;
		~napi_callback_info_memo() = default;

		explicit napi_callback_info_memo(const Napi::CallbackInfo& info, environment& ienv) :
				info_{info},
				ienv_{&ienv} {}

		[[nodiscard]] auto ienv() const -> environment& {
			return *ienv_;
		}

		[[nodiscard]] auto napi_callback_info() const -> const Napi::CallbackInfo& {
			return info_;
		}

	private:
		const Napi::CallbackInfo& info_;
		environment* ienv_;
};

} // namespace ivm::napi

namespace ivm::value {

// Delegate Napi::Value to iv8 common acceptor
template <class Meta>
struct accept<Meta, Napi::Value> : non_copyable {
		accept() = delete;
		accept(const accept&) = delete;
		accept(accept&&) = delete;
		~accept() = default;
		auto operator=(const accept&) -> accept& = delete;
		auto operator=(accept&&) -> accept& = delete;

		explicit accept(environment& env) :
				env_{&env} {}

		// nb: This handles `direct_cast` from a v8 handle to napi handle
		template <class Type>
		auto operator()(value_tag /*tag*/, v8::Local<Type> value) const -> Napi::Value {
			return {env_->napi_env(), v8_to_napi(value)};
		}

		auto operator()(auto_tag auto tag, auto&& value) const -> Napi::Value {
			auto local = delegate_accept<v8::Local<v8::Value>>(
				*this,
				tag,
				std::forward<decltype(value)>(value),
				env_->isolate(),
				env_->context()
			);
			return (*this)(value_tag{}, local);
		}

	private:
		environment* env_;
};

// Napi function arguments to list
template <>
struct visit<ivm::napi::napi_callback_info_memo> {
		auto operator()(const ivm::napi::napi_callback_info_memo& info, const auto& accept) const -> decltype(auto) {
			return accept(vector_tag{}, ivm::napi::arguments{info.napi_callback_info()});
		}
};

// Delegate Napi::Value to iv8 common visitor
template <>
struct visit<Napi::Value> {
		auto operator()(Napi::Value value, auto&& accept) const -> decltype(auto) {
			auto visit_scope = delegated_visit_scope(value.Env());
			auto local = napi_to_v8<v8::Value>(value);
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
