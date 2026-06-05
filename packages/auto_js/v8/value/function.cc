export module v8_js:function_definitions;
import :accept;
import :function;
import :unmaybe;
import auto_js;
import std;
import v8;

namespace js::iv8 {

template <class Result = std::monostate>
auto Function::apply(context_lock_witness lock, auto&& args) -> Result {
	auto result = [ & ]() {
		using args_type = std::vector<v8::Local<v8::Value>>;
		auto handle_scope = v8::EscapableHandleScope{lock.isolate()};
		auto undefined = js::transfer_in_strict<v8::Local<v8::Value>>(std::monostate{}, lock);
		auto argv = js::transfer_in_strict<args_type>(std::forward<decltype(args)>(args), lock);
		auto result = unmaybe(v8::Function::Call(lock.context(), undefined, argv.size(), argv.data()));
		return handle_scope.Escape(result);
	}();
	return js::transfer_out<Result>(result, lock);
}

template <class Result = std::monostate>
auto Function::call(context_lock_witness lock, auto&&... args) -> Result {
	auto result = [ & ]() {
		using args_type = std::array<v8::Local<v8::Value>, sizeof...(args) + 1>;
		auto handle_scope = v8::EscapableHandleScope{lock.isolate()};
		auto argv = js::transfer_in_strict<args_type>(
			std::tuple_cat(
				std::tuple{std::monostate{}},
				std::forward_as_tuple(std::forward<decltype(args)>(args)...)
			),
			lock
		);
		return handle_scope.Escape(unmaybe(v8::Function::Call(lock.context(), argv[ 0 ], argv.size() - 1, argv.data() + 1)));
	}();
	return js::transfer_out<Result>(result, lock);
}

} // namespace js::iv8
