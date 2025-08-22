module;
#include <cassert>
#include <memory>
#include <stop_token>
#include <utility>
module isolated_v8;
import :foreground_runner;
import :remote_handle;
import v8_js;

namespace isolated_v8 {

thread_local agent::lock* current_agent_lock{};

// agent::lock
agent::lock::lock(js::iv8::isolate_lock_witness& witness, std::shared_ptr<agent_host> host) :
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

agent::lock::~lock() {
	current_agent_lock = previous_;
}

auto agent::lock::get_current() -> lock& {
	assert(current_agent_lock != nullptr);
	return *current_agent_lock;
}

// agent::agent
agent::agent(const std::shared_ptr<agent_host>& host, std::shared_ptr<agent_severable> severable) :
		host_{host},
		severable_{std::move(severable)} {}

} // namespace isolated_v8
