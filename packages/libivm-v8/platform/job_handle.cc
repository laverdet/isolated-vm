module;
#include <memory>
#include <mutex>
#include <thread>
export module ivm.isolated_v8:platform.job_handle;
import :agent;
import ivm.utility;
import v8;

// See: /v8/src/libplatform/default-job.cc

namespace ivm {

export class job_handle : public v8::JobHandle, public v8::JobDelegate {
	public:
		job_handle(std::unique_ptr<v8::JobTask> job_task) :
				thread{[ this, job_task = std::move(job_task) ]() {
					job_task->Run(this);
				}} {}
		virtual ~job_handle() = default;

		virtual void NotifyConcurrencyIncrease() {}

		virtual void Join() {
			thread.join();
			thread = {};
		};

		virtual void Cancel() {
			thread.join();
			thread = {};
		};

		virtual void CancelAndDetach() {
			thread.join();
			thread = {};
		};

		virtual bool IsActive() { return thread.get_id() == std::thread::id{}; };
		virtual bool IsValid() final { return thread.get_id() == std::thread::id{}; };
		virtual bool UpdatePriorityEnabled() const final { return false; }
		virtual void UpdatePriority(v8::TaskPriority new_priority) final {}

		// JobDelegate
		virtual bool ShouldYield() { return false; }
		virtual uint8_t GetTaskId() { return 0; };
		virtual bool IsJoiningThread() const { return false; }

	private:
		std::unique_ptr<v8::JobTask> job_task;
		std::thread thread;
};

} // namespace ivm
