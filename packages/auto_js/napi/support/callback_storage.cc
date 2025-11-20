module;
#include <memory>
#include <type_traits>
#include <utility>
export module napi_js:callback_storage;
import :api;
import :environment_fwd;

namespace js::napi {

namespace {
// Since napi runs each environment in its own thread we can store the environment data pointer
// here. We could also go through `napi_get_instance_data` but there is no assurance that the
// pointer type is the same. We assume the environment pointer will outlive the callback.
template <auto_environment Environment>
thread_local Environment* env_local = nullptr;
} // namespace

// Converts any invocable into a `napi_callback` and data pointer for use in napi API calls. The
// result is a `std::tuple` with `{ callback_ptr, data_ptr, finalizer }`. Finalizer is either a
// `nullptr` or a `std::unique_ptr<T>` which should be disposed of in a finalizer.
template <auto_environment Environment>
auto make_callback_storage(Environment& env, std::invocable<Environment&, const callback_info&> auto function) {
	using function_type = decltype(function);
	if constexpr (std::is_trivially_copyable_v<function_type>) {
		if constexpr (std::is_empty_v<function_type>) {
			// Constant expression function, expressed entirely in the type. `data` is the environment.
			const auto callback = napi_callback{[](napi_env nenv, napi_callback_info info) -> napi_value {
				const auto args = callback_info{nenv, info};
				auto invoke = function_type{};
				auto& env = *static_cast<Environment*>(args.data());
				return invoke(env, args);
			}};
			return std::tuple{callback, &env};
		} else if constexpr (sizeof(function_type) <= sizeof(void*)) {
			// Trivial function type containing less than or equal to a pointer size. `data` is the
			// function and environment is stored in a thread local.
			env_local<Environment> = &env;
			auto* data = reinterpret_cast<void*&>(function);
			const auto callback = napi_callback{[](napi_env nenv, napi_callback_info info) -> napi_value {
				const auto args = callback_info{nenv, info};
				auto* data = args.data();
				auto& invoke = reinterpret_cast<function_type&>(data);
				auto& env = *env_local<Environment>;
				return invoke(env, args);
			}};
			return std::tuple{callback, data};
		} else {
			static_assert(false, "Large trivial function?");
		}
	} else {
		// Otherwise the function requires bound state w/ a destructor
		using pair_type = std::pair<Environment*, function_type>;
		auto external_data = std::make_unique<pair_type>(&env, std::move(function));
		const auto callback = napi_callback{[](napi_env nenv, napi_callback_info info) -> napi_value {
			const auto args = callback_info{nenv, info};
			auto& state = *static_cast<pair_type*>(args.data());
			return state.second(*state.first, args);
		}};
		return std::tuple{callback, std::move(external_data)};
	}
}

} // namespace js::napi
