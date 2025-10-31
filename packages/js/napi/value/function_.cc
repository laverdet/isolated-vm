module;
#include <span>
#include <variant>
export module napi_js:function;
import :environment_fwd;
import :primitive;
import :value_handle;
import ivm.utility;

namespace js::napi {

template <>
class value<function_tag> : public value_next<function_tag> {
	public:
		using value_next<function_tag>::value_next;

		template <class Result = std::monostate>
		auto apply(auto_environment auto& env, auto&& args) -> Result;

		template <class Result = std::monostate>
		auto call(auto_environment auto& env, auto&&... args) -> Result;

		template <auto_environment Environment>
		static auto make(Environment& env, auto function) -> value<function_tag>;

	private:
		template <class Result>
		auto invoke(auto_environment auto& env, std::span<napi_value> args) -> Result;
};

} // namespace js::napi
