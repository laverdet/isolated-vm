module;
#include <bit>
#include <cstring>
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
export template <auto_environment Environment>
auto make_napi_callback(Environment& env, std::invocable<Environment&, const callback_info&> auto function) {
	using function_type = std::remove_cvref_t<decltype(function)>;
	if constexpr (std::is_empty_v<function_type>) {
		// Constant expression function, expressed entirely in the type. `data` is the environment.
		static_assert(std::is_trivially_constructible_v<function_type>);
		const auto callback = napi_callback{[](napi_env nenv, napi_callback_info info) -> napi_value {
			const auto args = callback_info{nenv, info};
			auto invoke = function_type{};
			auto& env = *static_cast<Environment*>(args.data());
			return invoke(env, args);
		}};
		return std::tuple{callback, static_cast<void*>(&env), nullptr};
	} else if constexpr (sizeof(function_type) == sizeof(void*) && std::is_trivially_copyable_v<function_type>) {
		// Trivial function type containing only a pointer size, which is probably function pointer.
		// `data` is the function and environment is stored in a thread local.
		env_local<Environment> = &env;
		const auto callback = napi_callback{[](napi_env nenv, napi_callback_info info) -> napi_value {
			const auto args = callback_info{nenv, info};
			auto invoke = std::bit_cast<function_type>(args.data());
			auto& env = *env_local<Environment>;
			return invoke(env, args);
		}};
		return std::tuple{callback, std::bit_cast<void*>(function), nullptr};
	} else if constexpr (sizeof(function_type) <= sizeof(void*) && std::is_trivially_copyable_v<function_type>) {
		// This branch is currently unused but works fine. The assertion is here because it's probably a
		// mistake to have a function like this.
		static_assert(false);
		// Trivial function type which is non-empty, but smaller than a pointer. I would imagine that
		// this compiles down to the same thing as the branch above, which uses `std::bit_cast` instead
		// of `std::memcpy`.
		env_local<Environment> = &env;
		const auto callback = napi_callback{[](napi_env nenv, napi_callback_info info) -> napi_value {
			const auto args = callback_info{nenv, info};
			auto& invoke = *static_cast<function_type*>(args.data());
			auto& env = *env_local<Environment>;
			return invoke(env, args);
		}};
		const void* data{};
		std::memcpy(&data, &function, sizeof(function_type));
		return std::tuple{callback, data, nullptr};
	} else {
		// Otherwise the function requires bound state. A holder is used which contains the environment
		// & function data.
		struct holder {
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
				holder(Environment& env, function_type function) :
						env{&env},
						function{std::move(function)} {}
				Environment* env;
				function_type function;
		};
		// Callback definition
		const auto callback = napi_callback{[](napi_env nenv, napi_callback_info info) -> napi_value {
			const auto args = callback_info{nenv, info};
			auto& holder_ref = *static_cast<holder*>(args.data());
			return holder_ref.function(*holder_ref.env, args);
		}};
		// Make function, stashed with the holder ref
		auto holder_ptr = std::make_unique<holder>(env, std::move(function));
		return std::tuple{callback, static_cast<void*>(holder_ptr.get()), std::move(holder_ptr)};
	}
}

} // namespace js::napi
