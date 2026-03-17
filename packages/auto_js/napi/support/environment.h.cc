module;
#include <functional>
#include <memory>
export module napi_js:environment;
export import :environment_fwd;
import :api;
import util;

namespace js::napi {

// Internal holder for an environment pointer. Used as a common base class for `visit` & `accept`.
// This could maybe become something similar to `realm_scope` if the need for it is more common.
export template <auto_environment Type>
class environment_scope {
	public:
		explicit environment_scope(Type& env) : env_{env} {}

		explicit operator napi_env() const { return napi_env{env_.get()}; }
		[[nodiscard]] auto environment() const -> Type& { return env_; }

	private:
		std::reference_wrapper<Type> env_;
};

// A reference to an environment is used as the lock witness. Generally, you should not have an
// `environment&` unless you're in the napi thread and locked.
export class environment : util::non_moveable, public uv_schedulable {
	public:
		explicit environment(napi_env env);

		// NOLINTNEXTLINE(google-explicit-constructor)
		[[nodiscard]] operator napi_env() const { return env_; }
		[[nodiscard]] auto uses_direct_handles() const -> bool { return uses_direct_handles_; }

		template <class Type>
		static auto make_and_set_environment(napi_env env, auto&&... args) -> Type&
			requires std::constructible_from<Type, napi_env, decltype(args)...> {
			auto instance = std::make_unique<Type>(env, std::forward<decltype(args)>(args)...);
			return *apply_finalizer(std::move(instance), [ & ](Type* instance, napi_finalize finalize, void* hint) -> Type* {
				napi::invoke0(napi_set_instance_data, env, instance, finalize, hint);
				return instance;
			});
		}

		template <class Type>
		static auto unsafe_get_environment_as(napi_env env) -> Type& {
			return *static_cast<Type*>(napi::invoke(napi_get_instance_data, env));
		}

	private:
		napi_env env_;
		napi_async_cleanup_hook_handle cleanup_hook_handle_{};
		bool uses_direct_handles_;
};

} // namespace js::napi
