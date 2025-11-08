module;
#include <expected>
#include <utility>
export module v8_js:evaluation.script;
import :accept;
import :evaluation.origin;
import :lock;
import isolated_js;
import v8;

namespace js::iv8 {

export struct script {
	public:
		using expected_script_type = std::expected<v8::Local<v8::UnboundScript>, js::error_value>;
		using expected_value_type = std::expected<js::value_t, js::error_value>;

		// nb: It is undocumented (and even mentions "context independent"), but the script compiler
		// actually needs a context because it can throw an error and *that* would need a context.
		static auto compile(context_lock_witness lock, auto code_string, source_origin source_origin) -> expected_script_type;
		static auto run(context_lock_witness lock, v8::Local<v8::UnboundScript> script) -> expected_value_type;

	private:
		static auto compile(context_lock_witness lock, v8::Local<v8::String> code_string, source_origin source_origin) -> expected_script_type;
};

// ---

auto script::compile(context_lock_witness lock, auto code_string, source_origin source_origin) -> expected_script_type {
	auto local_code_string = js::transfer_in_strict<v8::Local<v8::String>>(std::move(code_string), lock);
	return compile(lock, local_code_string, std::move(source_origin));
}

} // namespace js::iv8
