export module v8_js:callback;
export import :callback_storage;
import :error;
import std;
import util;
import v8;

namespace js::iv8 {

template <class Type>
struct revive_lock_type : std::type_identity<Type> {};

template <class Agent, class... Implements>
struct revive_lock_type<isolate_lock_witness_of<Agent, Implements...>>
		: std::type_identity<const context_lock_witness_of<Agent, Implements...>&> {};

template <class Agent, class... Implements>
struct revive_lock_type<context_lock_witness_of<Agent, Implements...>>
		: std::type_identity<const context_lock_witness_of<Agent, Implements...>&> {};

// Make callback for plain free function
template <class Lock>
constexpr auto make_free_function(auto function) {
	using lock_type = revive_lock_type<Lock>::type;

	constexpr auto make_with_try_catch =
		[]<std::constructible_from<lock_type> LockAs, class... Args, bool Nx, class Result>(
			std::type_identity<auto(LockAs, Args...) noexcept(Nx)->Result> /*signature*/,
			auto callback
		) -> auto {
		using callback_type = decltype(callback);
		return util::bind{
			[](callback_type& callback, lock_type lock, const v8::FunctionCallbackInfo<v8::Value>& info) noexcept(Nx) -> void {
				// NOLINTNEXTLINE(cppcoreguidelines-slicing)
				auto result = invoke_internal_error_scope(lock, [ & ]() -> auto {
					auto run = util::regular_return{[ & ]() -> decltype(auto) {
						return std::apply(
							callback,
							std::tuple_cat(
								std::forward_as_tuple(lock),
								js::transfer_out<std::tuple<Args...>>(info, lock)
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
	};
	constexpr auto make_noexcept =
		[]<std::constructible_from<lock_type> LockAs, class Result>(
			std::type_identity<auto(LockAs) noexcept -> Result> /*signature*/,
			auto callback
		) -> auto {
		static_assert(false, "untested");
		using callback_type = decltype(callback);
		return util::bind{
			[](callback_type& callback, lock_type lock, const v8::FunctionCallbackInfo<v8::Value>& /*info*/) noexcept -> void {
				auto run = util::regular_return{[ & ]() -> decltype(auto) {
					return callback(lock);
				}};
				return js::transfer_in_strict<v8::Local<v8::Value>>(run().value_or(std::monostate{}), lock);
			},
			std::move(callback),
		};
	};

	auto callback = js::functional::thunk_free_function<lock_type>(std::move(function));
	constexpr auto make = util::overloaded{make_with_try_catch, make_noexcept};
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
}

} // namespace js::iv8
