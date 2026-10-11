export module v8_js:function_definitions;
import :accept;
import :function;
import :unmaybe;
import auto_js;
import std;
import v8;

namespace js::iv8 {

template <class Result>
auto Function::apply(const auto& lock, auto&& args) -> Result {
	auto result = [ & ] {
		auto handle_scope = v8::EscapableHandleScope{lock.isolate()};
		auto argv = js::transfer_in_strict<std::vector<v8::Local<v8::Value>>>(std::forward<decltype(args)>(args), lock);
		return handle_scope.Escape(invoke(lock, std::span{argv}));
	}();
	return js::transfer_out<Result>(result, lock);
}

template <class Result>
auto Function::call(const auto& lock, auto&&... args) -> Result {
	auto result = [ & ] {
		using args_type = std::array<v8::Local<v8::Value>, sizeof...(args)>;
		auto handle_scope = v8::EscapableHandleScope{lock.isolate()};
		auto argv = js::transfer_in_strict<args_type>(std::forward_as_tuple(std::forward<decltype(args)>(args)...), lock);
		return handle_scope.Escape(invoke(lock, std::span{argv}));
	}();
	return js::transfer_out<Result>(result, lock);
}

auto Function::invoke(context_lock_witness lock, std::span<v8::Local<v8::Value>> args) -> v8::Local<v8::Value> {
	auto allow_js = v8::Isolate::AllowJavascriptExecutionScope{lock.isolate()};
	return unmaybe(v8::Function::Call(lock.context(), v8::Undefined(lock.isolate()), static_cast<int>(args.size()), args.data()));
}

} // namespace js::iv8
