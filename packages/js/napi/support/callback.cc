module;
#include <tuple>
#include <type_traits>
#include <utility>
export module napi_js:callback;
export import :callback_storage;
import :api;
import :bound_value;
import :environment_fwd;
import :error_scope;
import :object;
import :value_handle;
import ivm.utility;

namespace js::napi {
namespace {

// Unwrap `this` into the correct tagged external
template <class Type>
constexpr auto unwrap_this = [](auto_environment auto& env, napi_value this_arg) -> tagged_external_of<Type>& {
	auto object = value<object_tag>::from(napi::invoke(napi_coerce_to_object, napi_env{env}, this_arg));
	return js::transfer_out<tagged_external_of<Type>&>(object, env);
};

// Thunk for most functions including free function, static functions, and constructors. The first
// `env` parameter is optional for callbacks. This returns a function which always accepts `env`.
template <auto_environment Environment>
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
				[](Callback& callback, const Environment& /*env*/, Args... args) noexcept(Nx) -> Result {
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
template <auto_environment Environment>
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
				[](Callback& callback, Type& that, const Environment& /*env*/, Args... args) noexcept(Nx) -> Result {
					return callback(that, std::forward<Args>(args)...);
				},
				std::move(callback),
			};
		},
	};
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
};

} // namespace

// Function type for internal host object construction
using internal_constructor = util::function_ref<auto(napi_value)->napi_value>;

// Make callback for plain free function
export template <auto_environment Environment>
constexpr auto make_free_function(auto function) {
	auto callback = thunk_free_function<Environment>(std::move(function));
	using callback_type = decltype(callback);

	constexpr auto make =
		[]<class... Args, bool Nx, class Result>(
			std::type_identity<auto(Environment&, Args...) noexcept(Nx)->Result>,
			callback_type callback
		) -> auto {
		return util::bind{
			[](callback_type& callback, Environment& env, const callback_info& info) noexcept(Nx) -> napi_value {
				if constexpr (Nx && sizeof...(Args) == 0) {
					static_assert(false);
					// clang bug? The `false` assert works fine but the code below causes an error because the
					// arguments don't match.
					// return js::transfer_in_strict<napi_value>(callback(env), env);
				} else {
					return invoke_napi_error_scope(env, [ & ]() -> napi_value {
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
					});
				}
			},
			std::move(callback),
		};
	};
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
}

// Make callback for constructor invocations
export template <auto_environment Environment, class Type>
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
			// None of this is tested.
			static_assert(false);
			auto callback = thunk_free_function<Environment>(std::move(constructor));
			using callback_type = decltype(callback);

			constexpr auto make =
				[]<class... Args, bool Nx, class Result>(
					std::type_identity<auto(Environment&, Args...) noexcept(Nx)->Result>,
					callback_type callback
				) -> auto {
				return util::bind{
					[](callback_type& callback, Environment& env, const callback_info& info) noexcept(Nx) -> napi_value {
						const auto wrap = [ & ](auto derived_instance) -> napi_value {
							// Tag the result
							static_assert(std::is_base_of_v<js::tagged_external, Type>);
							napi::invoke0(napi_type_tag_object, napi_env{env}, info.this_arg(), &type_tag_for<js::tagged_external>);
							// Wrap w/ finalizer
							auto erased_instance = util::safe_pointer_upcast<tagged_external>(std::move(derived_instance));
							return apply_finalizer(std::move(erased_instance), [ & ](Type* instance, napi_finalize finalize, void* hint) -> napi_value {
								napi::invoke0(napi_wrap, napi_env{env}, info.this_arg(), instance, finalize, hint, nullptr);
								return info.this_arg();
							});
						};
						if constexpr (Nx && sizeof...(Args) == 0) {
							return wrap(callback(env, value<object_tag>::from(info.this_arg())));
						} else {
							return invoke_napi_error_scope(env, [ & ]() -> napi_value {
								auto instance = std::apply(
									callback,
									std::tuple_cat(
										std::forward_as_tuple(env, value<object_tag>::from(info.this_arg())),
										js::transfer_out<std::tuple<Args...>>(info.arguments(), env)
									)
								);
								return wrap(std::move(instance));
							});
						}
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
				if (napi::invoke(napi_coerce_to_bool, napi_env{env}, maybe_addr)) {
					// Check type tag for `internal_constructor`, which is a `function_ref` to the constructor
					// we actually want to invoke.
					maybe_addr = napi::invoke(napi_coerce_to_object, napi_env{env}, maybe_addr);
					if (napi::invoke(napi_check_object_type_tag, napi_env{env}, maybe_addr, &type_tag_for<internal_constructor>)) {
						auto* addr = napi::invoke(napi_get_value_external, napi_env{env}, maybe_addr);
						const auto& constructor = *static_cast<internal_constructor*>(addr);
						return constructor(info.this_arg());
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
export template <auto_environment Environment, class Type, class Method>
constexpr auto make_member_function(Method method) {
	auto callback = thunk_member_function<Environment>(std::move(method));
	using callback_type = decltype(callback);

	constexpr auto make =
		[]<class That, class... Args, bool Nx, class Result>(
			std::type_identity<auto(That&, Environment&, Args...) noexcept(Nx)->Result>,
			callback_type callback
		) -> auto {
		return util::bind{
			[](callback_type& callback, Environment& env, const callback_info& info) noexcept(Nx) -> napi_value {
				// nb: Member functions can always throw, since `this` counts as an argument
				return invoke_napi_error_scope(env, [ & ]() -> napi_value {
					auto run = util::regular_return{[ & ]() -> decltype(auto) {
						return std::apply(
							callback,
							std::tuple_cat(
								std::forward_as_tuple(unwrap_this<Type>(env, info.this_arg()), env),
								js::transfer_out<std::tuple<Args...>>(info.arguments(), env)
							)
						);
					}};
					return js::transfer_in_strict<napi_value>(run(), env);
				});
			},
			std::move(callback),
		};
	};
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
};

} // namespace js::napi
