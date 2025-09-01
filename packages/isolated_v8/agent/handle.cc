module;
#include <cassert>
#include <memory>
#include <utility>
module isolated_v8;
import :agent_handle;
import :cluster;
import :foreground_runner;
import :remote_handle;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

// agent_lock
agent_lock::agent_lock(js::iv8::isolate_lock_witness witness, agent_host& host) :
		isolate_lock_witness{witness},
		remote_handle_lock{host.remote_handle_lock()},
		host_{host} {
	assert(host.isolate() == witness.isolate());
}

} // namespace isolated_v8
