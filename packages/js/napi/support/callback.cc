module;
#include <bit>
#include <concepts>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>
export module napi_js.callback;
import napi_js.callback_info;
import nodejs;

namespace js::napi {

// Since napi runs each environment in its own thread we can store the environment data pointer
// here. We could also go through `napi_get_instance_data` but there is no assurance that the
// pointer type is the same. We assume the environment pointer will outlive the callback.
template <class Environment>
thread_local Environment* env_local = nullptr;

// Converts any invocable into a `napi_callback` and data pointer for use in napi API calls. The
// result is a `std::pair` where the first element is another pair of the callback pointer and data
// pointer. The second element is either a `nullptr` or a `std::unique_ptr<T>` which should be
// disposed of in a finalizer.
export template <class Environment>
auto make_napi_callback(Environment& env, std::invocable<Environment&, const callback_info&> auto function) {
	using function_type = std::decay_t<decltype(function)>;
	if constexpr (std::is_empty_v<function_type>) {
		// Constant expression function. `data` is the environment.
		static_assert(std::is_trivially_constructible_v<function_type>);
		const auto callback = napi_callback{[](napi_env nenv, napi_callback_info info) -> napi_value {
			const auto args = callback_info{nenv, info};
			const auto invoke = function_type{};
			auto& env = *static_cast<Environment*>(args.data());
			return invoke(env, args);
		}};
		auto callback_ptr = std::pair{callback, static_cast<void*>(&env)};
		return std::pair{std::move(callback_ptr), nullptr};
	} else if constexpr (sizeof(function_type) == sizeof(void*) && std::is_trivially_copyable_v<function_type>) {
		// Trivial function type containing only a pointer size, which is probably function pointer.
		// `data` is the function and environment is stored in a thread local.
		env_local<Environment> = &env;
		const auto callback = napi_callback{[](napi_env nenv, napi_callback_info info) -> napi_value {
			const auto args = callback_info{nenv, info};
			const auto invoke = std::bit_cast<function_type>(args.data());
			auto& env = *env_local<Environment>;
			return invoke(env, args);
		}};
		auto callback_ptr = std::pair{callback, std::bit_cast<void*>(function)};
		return std::pair{std::move(callback_ptr), nullptr};
	} else if constexpr (sizeof(function_type) <= sizeof(void*) && std::is_trivially_copyable_v<function_type>) {
		// Trivial function type which is non-empty, but smaller than a pointer. I would imagine that
		// this compiles down to the same thing as the branch above, which uses `std::bit_cast` instead
		// of `std::memcpy`.
		env_local<Environment> = &env;
		const auto callback = napi_callback{[](napi_env nenv, napi_callback_info info) -> napi_value {
			const auto args = callback_info{nenv, info};
			auto invoke = function_type{};
			std::memcpy(&invoke, args.data(), sizeof(function_type));
			auto& env = *env_local<Environment>;
			return invoke(env, args);
		}};
		void* data{};
		std::memcpy(&data, function, sizeof(function_type));
		auto callback_ptr = std::pair{callback, data};
		return std::pair{std::move(callback_ptr), nullptr};
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
		auto callback_ptr = std::pair{callback, static_cast<void*>(holder_ptr.get())};
		return std::pair{std::move(callback_ptr), std::move(holder_ptr)};
	}
}

} // namespace js::napi
