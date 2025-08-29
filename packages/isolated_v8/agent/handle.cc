module;
#include <cassert>
#include <concepts>
#include <functional>
#include <memory>
#include <stop_token>
#include <utility>
#include <variant>
module isolated_v8;
import :agent_handle;
import :cluster;
import :foreground_runner;
import :remote_handle;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

thread_local agent_lock* current_agent_lock{};

// agent_lock
agent_lock::agent_lock(js::iv8::isolate_lock_witness& witness, std::shared_ptr<agent_host> host) :
		isolate_lock_witness{witness},
		remote_handle_lock{
			std::shared_ptr<remote_handle_list>{host, &host->remote_handle_list()},
			[ self = std::weak_ptr{host} ](expired_remote_type remote) {
				if (auto host = self.lock()) {
					foreground_runner::schedule_handle_task(
						host->foreground_runner(),
						[ host = host.get(),
							remote = std::move(remote) ](
							const std::stop_token& /*stop_token*/
						) {
							auto lock = js::iv8::isolate_execution_lock{host->isolate()};
							remote->reset(lock);
							host->remote_handle_list().erase(*remote);
						}
					);
				}
			},
		},
		host_{std::move(host)},
		previous_{std::exchange(current_agent_lock, this)} {}

agent_lock::~agent_lock() {
	current_agent_lock = previous_;
}

auto agent_lock::get_current() -> agent_lock& {
	assert(current_agent_lock != nullptr);
	return *current_agent_lock;
}

// agent_handle
agent_handle::agent_handle(const std::shared_ptr<agent_host>& host, std::shared_ptr<agent_severable> severable) :
		host_{host},
		severable_{std::move(severable)} {}

} // namespace isolated_v8
