module;
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
export module v8_js:callback;
export import :callback_storage;
import :error;
import util;
import v8;

namespace js::iv8 {

// Make callback for plain free function
export template <class Lock>
constexpr auto make_free_function(auto function) {
	constexpr auto make_with_try_catch =
		[]<std::constructible_from<Lock> LockAs, class... Args, bool Nx, class Result>(
			std::type_identity<auto(LockAs, Args...) noexcept(Nx)->Result> /*signature*/,
			auto callback
		) -> auto {
		using callback_type = decltype(callback);
		return util::bind{
			[](callback_type& callback, Lock lock, const v8::FunctionCallbackInfo<v8::Value>& info) noexcept(Nx) -> void {
				auto result = invoke_with_error_scope(lock, [ & ]() {
					auto run = util::regular_return{[ & ]() -> decltype(auto) {
						return std::apply(
							callback,
							std::tuple_cat(
								std::forward_as_tuple(lock),
								js::transfer_out<std::tuple<Args...>>(info, lock)
							)
						);
					}};
					return run();
				});
				if (result) {
					js::transfer_in_strict<v8::ReturnValue<v8::Value>>((*std::move(result)).value_or(std::monostate{}), lock, info.GetReturnValue());
				}
			},
			std::move(callback),
		};
	};
	constexpr auto make_noexcept =
		[]<std::constructible_from<Lock> LockAs, class Result>(
			std::type_identity<auto(LockAs) noexcept -> Result> /*signature*/,
			auto callback
		) -> auto {
		static_assert(false, "untested");
		using callback_type = decltype(callback);
		return util::bind{
			[](callback_type& callback, Lock lock, const v8::FunctionCallbackInfo<v8::Value>& /*info*/) noexcept -> void {
				auto run = util::regular_return{[ & ]() -> decltype(auto) {
					return callback(lock);
				}};
				return js::transfer_in_strict<v8::Local<v8::Value>>(run().value_or(std::monostate{}), lock);
			},
			std::move(callback),
		};
	};

	auto callback = js::functional::thunk_free_function<Lock>(std::move(function));
	constexpr auto make = util::overloaded{make_with_try_catch, make_noexcept};
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
}

} // namespace js::iv8
