module;
#include <cassert>
#include <memory>
#include <stop_token>
#include <utility>
module isolated_v8.agent;
import isolated_v8.foreground_runner;
import isolated_v8.remote_handle;
import v8_js;

namespace isolated_v8 {

thread_local agent::lock* current_agent_lock{};

// agent::lock
agent::lock::lock(std::shared_ptr<agent::host> host) :
		isolate_managed_lock{host->isolate()},
		host_{std::move(host)},
		previous_{std::exchange(current_agent_lock, this)} {}

agent::lock::~lock() {
	current_agent_lock = previous_;
}

auto agent::lock::accept_remote_handle(remote_handle& remote) noexcept -> void {
	host_->remote_handle_list().insert(remote);
}

auto agent::lock::remote_expiration_task() const -> reset_handle_type {
	auto self = std::weak_ptr{host_};
	return [ self = std::move(self) ](expired_remote_type remote) {
		if (auto host = self.lock()) {
			foreground_runner::schedule_handle_task(
				host->foreground_runner(),
				[ host = host.get(),
					remote = std::move(remote) ](
					const std::stop_token& /*stop_token*/
				) {
					js::iv8::isolate_managed_lock lock{host->isolate()};
					remote->reset(lock);
					host->remote_handle_list().erase(*remote);
				}
			);
		}
	};
}

auto agent::lock::get_current() -> lock& {
	assert(current_agent_lock != nullptr);
	return *current_agent_lock;
}

// agent::agent
agent::agent(const std::shared_ptr<host>& host, std::shared_ptr<severable> severable_) :
		host_{host},
		severable_{std::move(severable_)} {}

// agent::severable
agent::severable::severable(std::shared_ptr<host> host) :
		host_{std::move(host)} {}

auto agent::severable::sever() -> void {
	host_->severable_.reset();
}

} // namespace isolated_v8
