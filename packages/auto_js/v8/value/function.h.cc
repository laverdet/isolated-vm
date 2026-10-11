export module v8_js:function;
import :lock;
import std;
import v8;

namespace js::iv8 {

class Function : public v8::Function {
	public:
		template <class Result = std::monostate>
		auto apply(const auto& lock, auto&& args) -> Result;

		template <class Result = std::monostate>
		auto call(const auto& lock, auto&&... args) -> Result;

		// Invokes with arguments which were already transferred
		auto invoke(context_lock_witness lock, std::span<v8::Local<v8::Value>> args) -> v8::Local<v8::Value>;
};

} // namespace js::iv8
