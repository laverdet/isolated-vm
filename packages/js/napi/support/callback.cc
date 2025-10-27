module;
#include <bit>
#include <concepts>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>
export module napi_js:callback;
export import :callback_info;
import :api;
import :bound_value;
import :object;
import :value;
import ivm.utility;
import nodejs;

namespace js::napi {

namespace {

// Since napi runs each environment in its own thread we can store the environment data pointer
// here. We could also go through `napi_get_instance_data` but there is no assurance that the
// pointer type is the same. We assume the environment pointer will outlive the callback.
template <class Environment>
thread_local Environment* env_local = nullptr;

// Thunk for most functions including free function, static functions, and constructors. The first
// `env` parameter is optional for callbacks. This returns a function which always accepts `env`.
template <class Environment>
constexpr auto thunk_free_function = []<class Callback>(Callback callback) -> auto {
	constexpr auto make = util::overloaded{
		// With `env` parameter
		[]<class... Args, bool Nx, class Result>(
			std::type_identity<auto(Environment&, Args...) noexcept(Nx)->Result> /*signature*/,
			Callback callback
		) -> auto {
			return callback;
		},
		[]<class... Args, bool Nx, class Result>(
			std::type_identity<auto(const Environment&, Args...) noexcept(Nx)->Result> /*signature*/,
			Callback callback
		) -> auto {
			return callback;
		},
		// Without `env` parameter
		[]<class... Args, bool Nx, class Result>(
			std::type_identity<auto(Args...) noexcept(Nx)->Result> /*signature*/,
			Callback callback
		) -> auto {
			return util::bind{
				[](Callback& callback, const Environment& /*env*/, Args... args) -> Result {
					return callback(std::forward<Args>(args)...);
				},
				std::move(callback),
			};
		},
	};
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
};

// This takes the place of `thunk_free_function` for member functions, where `env` is the second
// parameter.
template <class Environment>
constexpr auto thunk_member_function = []<class Callback>(Callback callback) -> auto {
	constexpr auto make = util::overloaded{
		// With `env` parameter
		[]<class Type, class... Args, bool Nx, class Result>(
			std::type_identity<auto(Type&, Environment&, Args...) noexcept(Nx)->Result> /*signature*/,
			Callback callback
		) -> auto {
			return callback;
		},
		[]<class Type, class... Args, bool Nx, class Result>(
			std::type_identity<auto(Type&, const Environment&, Args...) noexcept(Nx)->Result> /*signature*/,
			Callback callback
		) -> auto {
			return callback;
		},
		// Without `env` parameter
		[]<class Type, class... Args, bool Nx, class Result>(
			std::type_identity<auto(Type&, Args...) noexcept(Nx)->Result> /*signature*/,
			Callback callback
		) -> auto {
			return util::bind{
				[](Callback& callback, Type& that, const Environment& /*env*/, Args... args) -> Result {
					return callback(that, std::forward<Args>(args)...);
				},
				std::move(callback),
			};
		},
	};
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
};

// Unwrap `this` into the correct tagged external
template <class Type>
constexpr auto unwrap_this = [](auto& env, napi_value this_arg) -> tagged_external_of<Type>& {
	auto object = value<object_tag>::from(js::napi::invoke(napi_coerce_to_object, napi_env{env}, this_arg));
	return js::transfer_out<tagged_external_of<Type>&>(object, env);
};

} // namespace

// Make callback for plain free function
template <class Environment>
constexpr auto make_free_function(auto function) {
	auto callback = thunk_free_function<Environment>(std::move(function));
	using callback_type = decltype(callback);

	constexpr auto make =
		[]<class... Args, bool Nx, class Result>(
			std::type_identity<auto(Environment&, Args...) noexcept(Nx)->Result>,
			callback_type callback
		) -> auto {
		return util::bind{
			[](callback_type& callback, Environment& env, const callback_info& info) -> napi_value {
				auto run = util::regular_return{[ & ]() -> decltype(auto) {
					return std::apply(
						callback,
						std::tuple_cat(
							std::forward_as_tuple(env),
							js::transfer_out<std::tuple<Args...>>(info.arguments(), env)
						)
					);
				}};
				return js::transfer_in_strict<napi_value>(run(), env);
			},
			std::move(callback),
		};
	};
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
}

// Make callback for constructor invocations
template <class Environment, class Type>
constexpr auto make_constructor_function(auto constructor) {
	// First, a lambda is created which will actually invoke the supplied constructor function.
	constexpr auto make_invoke_runtime_constructor = util::overloaded{
		[](std::nullptr_t /*constructor*/) -> auto {
			return [](Environment& env, const callback_info& /*info*/) -> napi_value {
				napi::invoke0(napi_throw_type_error, napi_env{env}, nullptr, "Illegal constructor");
				return {};
			};
		},
		[](auto constructor) -> auto {
			auto callback = thunk_free_function<Environment>(std::move(constructor));
			using callback_type = decltype(callback);

			constexpr auto make =
				[]<class... Args, bool Nx, class Result>(
					std::type_identity<auto(Environment&, Args...) noexcept(Nx)->Result>,
					callback_type callback
				) -> auto {
				return util::bind{
					[](callback_type& callback, Environment& env, const callback_info& info) -> napi_value {
						auto derived_instance = std::apply(
							callback,
							std::tuple_cat(
								std::forward_as_tuple(env, value<object_tag>::from(info.this_arg())),
								js::transfer_out<std::tuple<Args...>>(info.arguments(), env)
							)
						);
						auto erased_instance = util::safe_pointer_upcast<tagged_external>(std::move(derived_instance));

						// Tag the result
						static_assert(std::is_base_of_v<js::tagged_external, Type>);
						napi::invoke0(napi_type_tag_object, napi_env{env}, info.this_arg(), &type_tag_for<js::tagged_external>);
						// Wrap w/ finalizer
						apply_finalizer(std::move(erased_instance), [ & ](Type* instance, napi_finalize finalize, void* hint) -> void {
							napi::invoke0(napi_wrap, napi_env{env}, info.this_arg(), instance, finalize, hint, nullptr);
						});
						return info.this_arg();
					},
					std::move(callback),
				};
			};
			return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
		},
	};
	auto runtime_constructor = make_invoke_runtime_constructor(std::move(constructor));
	using runtime_constructor_type = decltype(runtime_constructor);

	// This part is really silly. We want the ability to create runtime host values through private
	// construction not available to JavaScript. This is done with tagged external values.
	return util::bind{
		[](runtime_constructor_type& constructor, Environment& env, const callback_info& info) -> napi_value {
			auto arguments = info.arguments();
			if (arguments.size() > 0) {
				auto* maybe_addr = arguments.at(0);
				// Check truthy since null or undefined causes `napi_coerce_to_object` to throw
				if (js::napi::invoke(napi_coerce_to_bool, napi_env{env}, maybe_addr)) {
					// Check type tag for `internal_constructor`, which is a `function_ref` to the constructor
					// we actually want to invoke.
					maybe_addr = js::napi::invoke(napi_coerce_to_object, napi_env{env}, maybe_addr);
					if (js::napi::invoke(napi_check_object_type_tag, napi_env{env}, maybe_addr, &type_tag_for<internal_constructor>)) {
						auto* addr = js::napi::invoke(napi_get_value_external, napi_env{env}, maybe_addr);
						auto& constructor = *static_cast<internal_constructor*>(addr);
						constructor(info.this_arg());
						return info.this_arg();
					}
				}
			}
			// Invoke normal constructor
			return constructor(env, info);
		},
		std::move(runtime_constructor),
	};
};

// Make callback for method invocations
template <class Environment, class Type, class Method>
constexpr auto make_member_function(Method method) {
	auto callback = thunk_member_function<Environment>(std::move(method));
	using callback_type = decltype(callback);

	constexpr auto make =
		[]<class That, class... Args, bool Nx, class Result>(
			std::type_identity<auto(That&, Environment&, Args...) noexcept(Nx)->Result>,
			callback_type callback
		) -> auto {
		return util::bind{
			[](callback_type& callback, Environment& env, const callback_info& info) -> napi_value {
				auto run = util::regular_return{[ & ]() -> decltype(auto) {
					// nb: Member functions can always throw, since `this` counts as an argument
					return std::apply(
						callback,
						std::tuple_cat(
							std::forward_as_tuple(unwrap_this<Type>(env, info.this_arg()), env),
							js::transfer_out<std::tuple<Args...>>(info.arguments(), env)
						)
					);
				}};
				return js::transfer_in_strict<napi_value>(run(), env);
			},
			std::move(callback),
		};
	};
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
};

// Converts any invocable into a `napi_callback` and data pointer for use in napi API calls. The
// result is a `std::tuple` with `{ callback_ptr, data_ptr, finalizer }`. Finalizer is either a
// `nullptr` or a `std::unique_ptr<T>` which should be disposed of in a finalizer.
template <class Environment>
auto make_napi_callback(Environment& env, std::invocable<Environment&, const callback_info&> auto function) {
	using function_type = std::remove_cvref_t<decltype(function)>;
	if constexpr (std::is_empty_v<function_type>) {
		// Constant expression function, expressed entirely in the type. `data` is the environment.
		static_assert(std::is_trivially_constructible_v<function_type>);
		const auto callback = napi_callback{[](napi_env nenv, napi_callback_info info) -> napi_value {
			return invoke_napi_scope([ & ]() -> napi_value {
				const auto args = callback_info{nenv, info};
				auto invoke = function_type{};
				auto& env = *static_cast<Environment*>(args.data());
				return invoke(env, args);
			});
		}};
		return std::tuple{callback, static_cast<void*>(&env), nullptr};
	} else if constexpr (sizeof(function_type) == sizeof(void*) && std::is_trivially_copyable_v<function_type>) {
		// Trivial function type containing only a pointer size, which is probably function pointer.
		// `data` is the function and environment is stored in a thread local.
		env_local<Environment> = &env;
		const auto callback = napi_callback{[](napi_env nenv, napi_callback_info info) -> napi_value {
			return invoke_napi_scope([ & ]() -> napi_value {
				const auto args = callback_info{nenv, info};
				auto invoke = std::bit_cast<function_type>(args.data());
				auto& env = *env_local<Environment>;
				return invoke(env, args);
			});
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
			return invoke_napi_scope([ & ]() -> napi_value {
				const auto args = callback_info{nenv, info};
				auto& invoke = *static_cast<function_type*>(args.data());
				auto& env = *env_local<Environment>;
				return invoke(env, args);
			});
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
			return invoke_napi_scope([ & ]() -> napi_value {
				const auto args = callback_info{nenv, info};
				auto& holder_ref = *static_cast<holder*>(args.data());
				return holder_ref.function(*holder_ref.env, args);
			});
		}};
		// Make function, stashed with the holder ref
		auto holder_ptr = std::make_unique<holder>(env, std::move(function));
		return std::tuple{callback, static_cast<void*>(holder_ptr.get()), std::move(holder_ptr)};
	}
}

} // namespace js::napi
