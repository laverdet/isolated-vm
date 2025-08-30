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
agent_lock::agent_lock(const js::iv8::isolate_lock_witness& witness, agent_host& host) :
		isolate_lock_witness{witness},
		remote_handle_lock{host.remote_handle_lock()},
		host_{host} {
	assert(host.isolate() == witness.isolate());
}

// agent_handle
agent_handle::agent_handle(const std::shared_ptr<agent_host>& host, std::shared_ptr<agent_severable> severable) :
		host_{host},
		severable_{std::move(severable)} {}

} // namespace isolated_v8
