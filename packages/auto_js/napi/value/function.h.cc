export module napi_js:function;
import :lock;
import :primitive;
import std;

namespace js::napi {

class local_for_function : public local_next<function_tag> {
	public:
		template <class Result = std::monostate>
		auto apply(const auto& lock, auto&& args) -> Result;

		template <class Result = std::monostate>
		auto call(const auto& lock, auto&&... args) -> Result;

		// Invoke with arguments which were already transferred
		auto invoke(environment_lock_witness lock, std::span<napi_value> args) -> napi_value;

		template <class Environment>
		static auto make(const environment_lock_witness_of<Environment>& lock, auto function) -> local_of<function_tag>;
};

} // namespace js::napi
