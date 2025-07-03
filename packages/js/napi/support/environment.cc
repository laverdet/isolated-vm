module;
#include <memory>
export module napi_js:environment;
import :api;
import :finalizer;
import ivm.utility;

namespace js::napi {

// A reference to an environment is used as the lock witness. Generally, you should not have an
// `environment&` unless you're in the napi thread and locked.
export class environment {
	public:
		explicit environment(napi_env env) :
				env_{env} {}

		[[nodiscard]] explicit operator napi_env() const { return env_; }

	private:
		napi_env env_;
};

// CRTP helper to instantiate an environment and attach it to the napi context
export template <class Type>
class environment_of : public environment {
	protected:
		friend Type;
		explicit environment_of(napi_env env) :
				environment{env} {}

	public:
		static auto make(napi_env env, auto&&... args) -> Type&
			requires std::constructible_from<Type, napi_env, decltype(args)...> {
			auto instance = std::make_unique<Type>(env, std::forward<decltype(args)>(args)...);
			return *js::napi::apply_finalizer(std::move(instance), [ & ](Type* data, napi_finalize finalize, void* hint) {
				js::napi::invoke0(napi_set_instance_data, env, data, finalize, hint);
				return data;
			});
		}
};

export template <class Type>
concept auto_environment = std::derived_from<Type, environment>;

// Internal holder for an environment pointer. Used as a common base class for `visit` & `accept`.
// This could maybe become something similar to `realm_scope` if the need for it is more common.
export template <class Type>
class environment_scope {
	public:
		explicit environment_scope(Type& env) :
				env_{&env} {}

		explicit operator napi_env() const { return napi_env{*env_}; }
		auto environment() const -> Type& { return *env_; }

	private:
		Type* env_;
};

} // namespace js::napi
