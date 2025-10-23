module;
#include <expected>
#include <utility>
export module isolated_v8:script;
import :agent_host;
import :evaluation_origin;
import :realm;
import :remote;
import v8_js;
import isolated_js;
import ivm.utility;
import v8;

namespace isolated_v8 {

export using script_local_type = v8::Local<v8::UnboundScript>;
export using script_shared_remote_type = shared_remote<v8::UnboundScript>;

export auto compile_script(const agent_lock& agent, auto&& code_string, source_origin source_origin) -> std::expected<script_local_type, js::error_value>;
export auto run_script(const realm::scope& realm, script_local_type script) -> std::expected<js::value_t, js::error_value>;

// ---

auto compile_script_direct(const agent_lock& agent, v8::Local<v8::String> code_string, source_origin source_origin) -> std::expected<script_local_type, js::error_value>;

auto compile_script(const agent_lock& agent, auto&& code_string, source_origin source_origin) -> std::expected<script_local_type, js::error_value> {
	auto local_code_string = js::transfer_in_strict<v8::Local<v8::String>>(std::forward<decltype(code_string)>(code_string), agent);
	return compile_script_direct(agent, local_code_string, std::move(source_origin));
}

} // namespace isolated_v8
