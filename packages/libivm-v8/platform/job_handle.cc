module;
#include <memory>
#include <thread>
module ivm.isolated_v8;
import :platform;
import ivm.utility;
import v8;

// See: /v8/src/libplatform/default-job.cc

namespace isolated_v8 {

job_handle::job_handle(std::unique_ptr<v8::JobTask> job_task) :
		thread{[ this, job_task = std::move(job_task) ]() {
			job_task->Run(this);
		}} {}

auto job_handle::Cancel() -> void {
	thread.join();
	thread = {};
};

auto job_handle::CancelAndDetach() -> void {
	thread.join();
	thread = {};
};

auto job_handle::IsActive() -> bool {
	return thread.get_id() == std::thread::id{};
};

auto job_handle::IsValid() -> bool {
	return thread.get_id() == std::thread::id{};
};

auto job_handle::Join() -> void {
	thread.join();
	thread = {};
};

auto job_handle::NotifyConcurrencyIncrease() -> void {}

auto job_handle::UpdatePriority(v8::TaskPriority /*new_priority*/) -> void {
}

auto job_handle::UpdatePriorityEnabled() const -> bool {
	return false;
}

auto job_handle::GetTaskId() -> uint8_t {
	return 0;
};

auto job_handle::IsJoiningThread() const -> bool {
	return false;
}

auto job_handle::ShouldYield() -> bool {
	return false;
}

} // namespace isolated_v8
