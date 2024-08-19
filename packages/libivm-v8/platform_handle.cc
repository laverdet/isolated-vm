module;
#include <memory>
#include <mutex>
#include <utility>
export module ivm.isolated_v8:internal.platform_handle;
import :platform;
import ivm.utility;

namespace ivm {

// Responsible for initializing v8 and creating one `platform` per process. When the last handle is
// destroyed, the `platform` is destroyed and v8 is shut down.
export class platform_handle {
	public:
		platform_handle(const platform_handle&) = default;
		platform_handle(platform_handle&&) = default;
		auto operator=(const platform_handle&) -> platform_handle& = default;
		auto operator=(platform_handle&&) noexcept -> platform_handle& = default;
		~platform_handle();

		static auto acquire() -> platform_handle;

	private:
		explicit platform_handle(std::shared_ptr<platform> platform) :
				platform_{std::move(platform)} {}

		std::shared_ptr<platform> platform_;
};

lockable<std::weak_ptr<platform>, std::mutex> shared_platform{};

platform_handle::~platform_handle() {
	// nb: Platform must be released with the lock so that if one thread is releasing while another is
	// trying to acquire then v8 will fully shutdown and then start back up again.
	shared_platform.write()->reset();
}

auto platform_handle::acquire() -> platform_handle {
	auto lock = shared_platform.write();
	auto acquired_platform = lock->lock();
	if (!acquired_platform) {
		acquired_platform = std::make_shared<platform>();
		*lock = acquired_platform;
	}
	return platform_handle{std::move(acquired_platform)};
}

} // namespace ivm
