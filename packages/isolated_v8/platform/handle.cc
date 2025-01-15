module;
#include <memory>
#include <mutex>
#include <utility>
module ivm.isolated_v8;
import :platform;
import ivm.utility;

namespace isolated_v8 {

util::lockable<std::weak_ptr<platform>, std::mutex> shared_platform{};

platform::handle::handle(std::shared_ptr<platform> platform) :
		platform_{std::move(platform)} {}

platform::handle::~handle() {
	// nb: Platform must be released with the lock so that if one thread is releasing while another is
	// trying to acquire then v8 will fully shutdown and then start back up again.
	shared_platform.write()->reset();
}

auto platform::handle::acquire() -> handle {
	auto lock = shared_platform.write();
	auto acquired_platform = lock->lock();
	if (!acquired_platform) {
		acquired_platform = std::make_shared<platform>();
		*lock = acquired_platform;
	}
	return handle{std::move(acquired_platform)};
}

} // namespace isolated_v8
