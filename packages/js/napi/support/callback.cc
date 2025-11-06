module;
#include <optional>
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

// Unwrap `this` into the correct tagged external. Returns `std::nullopt` if there was a type error,
// and there is a napi exception pending. `nullptr` will not be returned.
template <class Type>
constexpr auto unwrap_member_this = [](auto_environment auto& env, napi_value this_arg) noexcept -> std::optional<tagged_external<Type>> {
	return napi::invoke_maybe(napi_coerce_to_object, napi_env{env}, this_arg)
		.and_then([ &env ](napi_value coerced_this_arg) -> std::optional<tagged_external<Type>> {
			auto as_object = bound_value{env, value<object_tag>::from(coerced_this_arg)};
			// Technically `try_cast` can throw but it should only be in catastrophic cases.
			Type* unwrapped = as_object.try_cast(type<Type>);
			if (unwrapped == nullptr) {
				// nb: The failure here is "no tag" which means we need to set the napi exception
				napi::invoke0(napi_throw_type_error, napi_env{env}, nullptr, "Invalid object type");
				return std::nullopt;
			} else {
				return tagged_external{*unwrapped};
			}
		});
};

} // namespace

// Function type for internal host object construction
using internal_constructor = util::function_ref<auto(napi_value)->napi_value>;

// Make callback for plain free function
export template <auto_environment Environment>
constexpr auto make_free_function(auto function) {
	constexpr auto make_with_try_catch =
		[]<class Env, class... Args, bool Nx, class Result>(
			std::type_identity<auto(Env, Args...) noexcept(Nx)->Result> /*signature*/,
			auto callback
		) -> auto {
		using callback_type = decltype(callback);
		return util::bind{
			[](callback_type& callback, Environment& env, const callback_info& info) noexcept(Nx) -> napi_value {
				return invoke_with_error_scope(env, [ & ]() -> napi_value {
					auto run = util::regular_return{[ & ]() -> decltype(auto) {
						return std::apply(
							callback,
							std::tuple_cat(
								std::forward_as_tuple(env),
								js::transfer_out<std::tuple<Args...>>(info.arguments(), env)
							)
						);
					}};
					return js::transfer_in_strict<napi_value>(run().value_or(std::monostate{}), env);
				});
			},
			std::move(callback),
		};
	};
	constexpr auto make_noexcept =
		[]<class Env, class Result>(
			std::type_identity<auto(Env) noexcept -> Result> /*signature*/,
			auto callback
		) -> auto {
		static_assert(false, "untested");
		using callback_type = decltype(callback);
		return util::bind{
			[](callback_type& callback, Environment& env, const callback_info& /*info*/) noexcept -> napi_value {
				auto run = util::regular_return{[ & ]() -> decltype(auto) {
					return callback(env);
				}};
				return js::transfer_in_strict<napi_value>(run().value_or(std::monostate{}), env);
			},
			std::move(callback),
		};
	};

	auto callback = js::functional::thunk_free_function<Environment&>(std::move(function));
	constexpr auto make = util::overloaded{make_with_try_catch, make_noexcept};
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
			// These are the last steps, common to both branches
			constexpr auto wrap = [](auto instance, Environment& env, const callback_info& info) -> napi_value {
				// Tag the result
				napi::invoke0(napi_type_tag_object, napi_env{env}, info.this_arg(), &type_tag_for<Type>);
				// Wrap w/ finalizer
				return apply_finalizer(std::move(instance), [ & ](Type* instance, napi_finalize finalize, void* hint) -> napi_value {
					napi::invoke0(napi_wrap, napi_env{env}, info.this_arg(), instance, finalize, hint, nullptr);
					return info.this_arg();
				});
			};
			using wrap_type = decltype(wrap);

			constexpr auto make_with_try_catch =
				[]<class Env, class... Args, bool Nx, class Result>(
					std::type_identity<auto(Env, Args...) noexcept(Nx)->Result> /*signature*/,
					auto callback
				) -> auto {
				static_assert(false, "untested");
				using callback_type = decltype(callback);
				return util::bind{
					[](callback_type& callback, wrap_type& wrap, Environment& env, const callback_info& info) noexcept(Nx) -> napi_value {
						return invoke_with_error_scope(env, [ & ]() -> napi_value {
							auto instance = std::apply(
								callback,
								std::tuple_cat(
									std::forward_as_tuple(env, value<object_tag>::from(info.this_arg())),
									js::transfer_out<std::tuple<Args...>>(info.arguments(), env)
								)
							);
							return wrap(std::move(instance));
						});
					},
					std::move(callback),
					wrap,
				};
			};
			constexpr auto make_noexcept =
				[]<class Env, class Result>(
					std::type_identity<auto(Env) noexcept -> Result> /*signature*/,
					auto callback
				) -> auto {
				static_assert(false, "untested");
				using callback_type = decltype(callback);
				return util::bind{
					[](callback_type& callback, Environment& env, const callback_info& info) noexcept -> napi_value {
						return wrap(callback(env, value<object_tag>::from(info.this_arg())));
					},
					std::move(callback),
				};
			};

			auto callback = js::functional::thunk_free_function<Environment&>(std::move(constructor));
			using signature_type = util::function_signature_t<decltype(callback)>;
			static_assert(util::signature_result<signature_type>{} != type<void>);
			constexpr auto make = util::overloaded{make_with_try_catch, make_noexcept};
			return make(std::type_identity<signature_type>{}, std::move(callback));
		},
	};
	auto runtime_constructor = make_invoke_runtime_constructor(std::move(constructor));
	using runtime_constructor_type = decltype(runtime_constructor);

	// This part is really silly. We want the ability to create runtime host values through direct C++
	// invocation not available to JavaScript. This is done with a tagged external value to a
	// `util::function_ref`.
	return util::bind{
		[](runtime_constructor_type& constructor, Environment& env, const callback_info& info) -> napi_value {
			auto arguments = info.arguments();
			if (arguments.size() > 0) {
				auto maybe_constructor = [ & ]() -> std::optional<internal_constructor*> {
					try {
						// Check truthiness since null or undefined causes `napi_coerce_to_object` to throw
						auto* arg0 = arguments.at(0);
						if (napi::invoke(napi_coerce_to_bool, napi_env{env}, arg0)) {
							auto* as_object = napi::invoke(napi_coerce_to_object, napi_env{env}, arg0);
							auto has_tag = napi::invoke(napi_check_object_type_tag, napi_env{env}, as_object, &type_tag_for<internal_constructor>);
							if (has_tag) {
								void* addr = napi::invoke(napi_get_value_external, napi_env{env}, as_object);
								return static_cast<internal_constructor*>(addr);
							}
						}
						return nullptr;
					} catch (const napi::pending_error& /*error*/) {
						return std::nullopt;
					}
				}();
				if (!maybe_constructor) {
					return nullptr;
				}
				if (*maybe_constructor != nullptr) {
					const auto& constructor = **maybe_constructor;
					return invoke_with_error_scope(env, [ & ]() -> napi_value {
						return constructor(info.this_arg());
					});
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
	constexpr auto make_with_try_catch =
		[]<class That, class Env, class... Args, bool Nx, class Result>(
			std::type_identity<auto(That, Env, Args...) noexcept(Nx)->Result> /*signature*/,
			auto callback
		) -> auto {
		using callback_type = decltype(callback);
		return util::bind{
			[](callback_type& callback, Environment& env, const callback_info& info) noexcept(Nx) -> napi_value {
				auto maybe_that = unwrap_member_this<Type>(env, info.this_arg());
				if (!maybe_that) {
					return nullptr;
				}
				return invoke_with_error_scope(env, [ & ]() -> napi_value {
					auto run = util::regular_return{[ & ]() -> decltype(auto) {
						return std::apply(
							callback,
							std::tuple_cat(
								std::forward_as_tuple(**maybe_that, env),
								js::transfer_out<std::tuple<Args...>>(info.arguments(), env)
							)
						);
					}};
					return js::transfer_in_strict<napi_value>(run().value_or(std::monostate{}), env);
				});
			},
			std::move(callback),
		};
	};
	constexpr auto make_noexcept =
		[]<class That, class Env, class Result>(
			std::type_identity<auto(That, Env) noexcept -> Result> /*signature*/,
			auto callback
		) -> auto {
		static_assert(false, "untested");
		using callback_type = decltype(callback);
		return util::bind{
			[](callback_type& callback, Environment& env, const callback_info& info) noexcept -> napi_value {
				auto maybe_that = unwrap_member_this<Type>(env, info.this_arg());
				if (!maybe_that) {
					return nullptr;
				}
				auto run = util::regular_return{[ & ]() -> decltype(auto) {
					return callback(**maybe_that, env);
				}};
				return js::transfer_in_strict<napi_value>(run().value_or(std::monostate{}), env);
			},
			std::move(callback),
		};
	};

	auto callback = js::functional::thunk_member_function<Environment&>(std::move(method));
	constexpr auto make = util::overloaded{make_with_try_catch, make_noexcept};
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
};

} // namespace js::napi
