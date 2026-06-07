export module isolated_vm:value.function_definitions;
import :transfer.accept;
import :transfer.visit;

namespace isolated_vm {
using namespace js;

template <class Result = std::monostate>
auto value_for_function::apply(const runtime_lock& lock, auto&& args) -> Result {
	using args_type = std::vector<value_of<>>;
	auto undefined = js::transfer_in_strict<value_of<>>(std::monostate{}, lock);
	auto argv = js::transfer_in_strict<args_type>(std::forward<decltype(args)>(args), lock);
	auto result = invoke(lock, undefined, argv);
	return js::transfer_out<Result>(result, lock);
}

template <class Result = std::monostate>
auto value_for_function::call(const runtime_lock& lock, auto&&... args) -> Result {
	using args_type = std::array<value_of<>, sizeof...(args) + 1>;
	auto argv = js::transfer_in_strict<args_type>(
		std::tuple_cat(
			std::tuple{std::monostate{}},
			std::forward_as_tuple(std::forward<decltype(args)>(args)...)
		),
		lock
	);
	auto result = invoke(lock, argv[ 0 ], std::span{argv}.subspan(1));
	return js::transfer_out<Result>(result, lock);
}

} // namespace isolated_vm
