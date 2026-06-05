export module v8_js:function;
import :lock;
import v8;

namespace js::iv8 {

class Function : public v8::Function {
	public:
		template <class Result = std::monostate>
		auto apply(context_lock_witness lock, auto&& args) -> Result;

		template <class Result = std::monostate>
		auto call(context_lock_witness lock, auto&&... args) -> Result;
};

} // namespace js::iv8
