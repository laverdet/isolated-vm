export module isolated_vm:support.free_function;
export import :support.callback_storage;
import std;
import util;

namespace isolated_vm {

// Make callback for plain free function
template <class Lock>
constexpr auto make_free_function(auto function) {
	constexpr auto make_with_try_catch =
		[]<std::constructible_from<Lock> LockAs, class... Args, bool Nx, class Result>(
			std::type_identity<auto(LockAs, Args...) noexcept(Nx)->Result> /*signature*/,
			auto callback
		) -> auto {
		using callback_type = decltype(callback);
		auto bound_function = util::bind{
			[](const callback_type& callback, Lock lock, callback_info info) noexcept(Nx) -> value_of<> {
				auto run = util::regular_return{[ & ]() -> decltype(auto) {
					return std::apply(
						callback,
						std::tuple_cat(
							std::forward_as_tuple(lock),
							js::transfer_out<std::tuple<Args...>>(info, lock)
						)
					);
				}};
				return js::transfer_in_strict<value_of<>>(run().value_or(std::monostate{}), lock);
			},
			std::move(callback),
		};
		return std::tuple{std::move(bound_function), sizeof...(Args)};
	};

	auto callback = js::functional::thunk_free_function<Lock>(std::move(function));
	constexpr auto make = make_with_try_catch;
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
}

} // namespace isolated_vm
