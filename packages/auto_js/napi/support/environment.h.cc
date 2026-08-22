export module napi_js:environment;
export import :environment_fwd;
import :api;
import std;
import util;
import v8;

namespace js::napi {

// Per-instance environment state. The lifetime of instances of this class is managed by the
// runtime.
export class environment : util::non_moveable, public napi_schedulable {
	public:
		explicit environment(napi_env env);

		// NOLINTNEXTLINE(google-explicit-constructor)
		[[nodiscard]] operator napi_env() const { return env_; }

		// In the fast (nodejs) case this is the isolate which backs the environment. Otherwise it is
		// `nullptr`.
		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }

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
		v8::Isolate* isolate_{};
		napi_async_cleanup_hook_handle cleanup_hook_handle_{};
};

} // namespace js::napi
