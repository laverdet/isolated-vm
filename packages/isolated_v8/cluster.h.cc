module;
#include <algorithm>
#include <list>
#include <memory>
export module isolated_v8:cluster;
import :isolated_platform;
import ivm.utility;
import v8_js;
import v8;

namespace isolated_v8 {

// Owns a group of `agent` instances. There's one cluster per nodejs context (worker_thread).
// `cluster` is ultimately the owner of all agents it creates.
export class cluster : util::non_moveable {
	public:
		cluster() :
				platform_handle_{isolated_platform::acquire()} {}

		auto acquire_runner() -> std::shared_ptr<js::iv8::platform::foreground_runner>;
		auto release_runner(const std::shared_ptr<js::iv8::platform::foreground_runner>& runner) -> void;

	private:
		js::iv8::platform::platform_handle platform_handle_;
		util::lockable<std::list<std::shared_ptr<js::iv8::platform::foreground_runner>>> storage_;
};

// ---

auto cluster::acquire_runner() -> std::shared_ptr<js::iv8::platform::foreground_runner> {
	return storage_.write()->emplace_back(std::make_shared<js::iv8::platform::foreground_runner>());
}

auto cluster::release_runner(const std::shared_ptr<js::iv8::platform::foreground_runner>& runner) -> void {
	auto lock = storage_.write();
	auto it = std::ranges::find_if(*lock, [ & ](const auto& pointer) -> bool {
		return pointer == runner;
	});
	lock->erase(it);
}

} // namespace isolated_v8
