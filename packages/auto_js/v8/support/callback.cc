export module v8_js:callback;
export import :callback_storage;
import :error;
import std;
import util;
import v8;

namespace js::iv8 {

// Convert incoming template lock type into runtime context lock type.
template <class>
struct revive_lock_type : std::type_identity<context_lock_witness> {};

template <class Agent, class... Implements>
struct revive_lock_type<isolate_lock_witness_of<Agent, Implements...>>
		: std::type_identity<const context_lock_witness_of<Agent, Implements...>&> {};

template <class Agent, class... Implements>
struct revive_lock_type<context_lock_witness_of<Agent, Implements...>>
		: std::type_identity<const context_lock_witness_of<Agent, Implements...>&> {};

// Free function callback maker without noexcept
constexpr auto make_free_function_with_try_catch =
	[]<class Lock, class... Args, bool Nx, class Result>(
		std::type_identity<auto(Lock, Args...) noexcept(Nx)->Result> /*signature*/,
		auto callback
	) -> auto {
	using callback_type = decltype(callback);
	auto bound_function = util::bind{
		[](callback_type& callback, Lock lock, const v8::FunctionCallbackInfo<v8::Value>& info) noexcept(Nx) -> void {
			// NOLINTNEXTLINE(cppcoreguidelines-slicing)
			auto result = invoke_internal_error_scope(lock, [ & ]() -> auto {
				auto run = util::regular_return{[ & ]() -> decltype(auto) {
					return std::apply(
						callback,
						std::tuple_cat(
							std::forward_as_tuple(lock),
							js::transfer_out<std::tuple<js::functional::parameter_transfer_as_t<Args>...>>(info, lock)
						)
					);
				}};
				return run().value_or(std::monostate{});
			});
			if (result) {
				return_into(lock, info.GetReturnValue(), *std::move(result));
			}
		},
		std::move(callback),
	};
	return std::tuple{std::move(bound_function), sizeof...(Args)};
};

// Free function callback maker which can skip try/catch if there are no params, and it is also
// marked as noexcept.
constexpr auto make_free_function_noexcept =
	[]<class Lock, class Result>(
		std::type_identity<auto(Lock) noexcept -> Result> /*signature*/,
		auto callback
	) -> auto {
	static_assert(false, "untested");
	using callback_type = decltype(callback);
	auto bound_function = util::bind{
		[](callback_type& callback, Lock lock, const v8::FunctionCallbackInfo<v8::Value>& /*info*/) noexcept -> void {
			auto run = util::regular_return{[ & ]() -> decltype(auto) {
				return callback(lock);
			}};
			return js::transfer_in_strict<v8::Local<v8::Value>>(run().value_or(std::monostate{}), lock);
		},
		std::move(callback),
	};
	return std::tuple{std::move(bound_function), 0};
};

// Make callback for plain free function
template <class Lock>
constexpr auto make_free_function(auto function) {
	constexpr auto make = util::overloaded{make_free_function_with_try_catch, make_free_function_noexcept};
	using revived_lock_type = revive_lock_type<Lock>::type;
	auto callback = js::functional::thunk_free_function<revived_lock_type, context_lock_witness>(std::move(function));
	using signature_type = util::function_signature_t<decltype(callback)>;
	return make(std::type_identity<signature_type>{}, std::move(callback));
}

} // namespace js::iv8
