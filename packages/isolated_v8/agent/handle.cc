module isolated_v8;
import :agent_handle;
import :cluster;
import :foreground_runner;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

// `agent_remote_handle_lock`
agent_remote_handle_lock::agent_remote_handle_lock(js::iv8::isolate_lock_witness lock, agent_host& agent) :
		remote_handle_lock{agent.remote_handle_lock(lock)} {}

// `agent_collected_handle_lock`
agent_collected_handle_lock::agent_collected_handle_lock(js::iv8::isolate_lock_witness lock, agent_host& agent) :
		collected_handle_lock{lock, agent.autorelease_pool()} {};

// `agent_module_specifiers_lock`
agent_module_specifiers_lock::agent_module_specifiers_lock(js::iv8::context_lock_witness lock, agent_host& agent) :
		module_specifiers_lock{lock, agent.weak_module_specifiers()} {};

} // namespace isolated_v8
