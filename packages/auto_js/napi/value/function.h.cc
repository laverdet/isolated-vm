export module napi_js:function;
import :primitive;
import std;

namespace js::napi {

class value_for_function : public value_next<function_tag> {
	public:
		template <class Result = std::monostate>
		auto apply(auto_environment auto& env, auto&& args) -> Result;

		template <class Result = std::monostate>
		auto call(auto_environment auto& env, auto&&... args) -> Result;

		template <auto_environment Environment>
		static auto make(Environment& env, auto function) -> value_of<function_tag>;

	private:
		template <class Result>
		auto invoke(auto_environment auto& env, std::span<napi_value> args) -> Result;
};

} // namespace js::napi
