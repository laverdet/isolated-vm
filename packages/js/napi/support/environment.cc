module;
#include <memory>
#include <stdexcept>
export module napi_js:environment;
import :api;
import :finalizer;
import :utility;
import :uv_scheduler;
import ivm.utility;

namespace js::napi {

// A reference to an environment is used as the lock witness. Generally, you should not have an
// `environment&` unless you're in the napi thread and locked.
export class environment : util::non_moveable, public uv_schedulable {
	public:
		explicit environment(napi_env env) :
				env_{env},
				uses_direct_handles_{[ & ]() -> bool {
					direct_address_equal direct_equal{};
					indirect_address_equal indirect_equal{};
					auto* array = napi::invoke(napi_create_array_with_length, env, 1);
					napi::invoke0(napi_set_element, env, array, 0, array);
					auto* result = napi::invoke(napi_get_element, env, array, 0);
					if (direct_equal(array, result)) {
						return true;
					} else if (indirect_equal(array, result)) {
						return false;
					} else {
						throw std::runtime_error{"Exotic napi handle behavior detected"};
					}
				}()} {
			scheduler().open(js::napi::invoke(napi_get_uv_event_loop, env));
		}

		~environment() {
			scheduler().close();
		}

		[[nodiscard]] explicit operator napi_env() const { return env_; }
		[[nodiscard]] auto uses_direct_handles() const -> bool { return uses_direct_handles_; }

	private:
		napi_env env_;
		bool uses_direct_handles_;
};

// CRTP helper to instantiate an environment and attach it to the napi context
export template <class Type>
class environment_of : public environment {
	protected:
		friend Type;
		explicit environment_of(napi_env env) :
				environment{env} {}

	public:
		static auto unsafe_get(napi_env env) -> Type& {
			return *static_cast<Type*>(js::napi::invoke(napi_get_instance_data, env));
		}

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
