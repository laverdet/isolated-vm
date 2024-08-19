module;
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stop_token>
export module ivm.isolated_v8:agent;
import :agent.fwd;
import :agent.lock;
import v8;
import ivm.utility;

namespace ivm {

struct isolate_destructor {
		auto operator()(v8::Isolate* isolate) -> void;
};

class agent::host : non_moveable {
	public:
		friend agent;
		explicit host(std::shared_ptr<storage> agent_storage);

		auto execute(const std::stop_token& stop_token) -> void;
		auto isolate() -> v8::Isolate*;
		auto scratch_context() -> v8::Local<v8::Context>;

	private:
		std::shared_ptr<storage> agent_storage;
		std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator;
		std::unique_ptr<v8::Isolate, isolate_destructor> isolate_;
		v8::Global<v8::Context> scratch_context_;
		lockable<std::vector<task_type>, std::mutex, std::condition_variable_any> pending_tasks_;
};

agent::host::host(std::shared_ptr<storage> agent_storage) :
		agent_storage{std::move(agent_storage)},
		array_buffer_allocator{v8::ArrayBuffer::Allocator::NewDefaultAllocator()},
		isolate_{v8::Isolate::Allocate()} {
	auto create_params = v8::Isolate::CreateParams{};
	create_params.array_buffer_allocator = array_buffer_allocator.get();
	v8::Isolate::Initialize(isolate_.get(), create_params);
}

auto agent::host::execute(const std::stop_token& stop_token) -> void {
	// Enter isolate on this thread
	const auto locker = v8::Locker{isolate_.get()};
	const auto isolate_scope = v8::Isolate::Scope{isolate_.get()};
	// Exclusive lock on tasks
	auto lock = pending_tasks_.write_waitable([](const std::vector<task_type>& tasks) {
		return !tasks.empty();
	});
	do {
		// Accept task list locally, then unlock
		auto tasks = std::move(*lock);
		lock.unlock();
		// Dispatch all tasks
		auto agent_lock = agent::lock{*this};
		for (auto& task : tasks) {
			const auto handle_scope = v8::HandleScope{isolate_.get()};
			take(std::move(task))(agent_lock);
		}
		// Unlock v8 isolate before suspendiconst ng
		const auto unlocker = v8::Unlocker{isolate_.get()};
		// Wait for more tasks
		lock.lock();
	} while (lock.wait(stop_token));
}

auto agent::host::isolate() -> v8::Isolate* {
	return isolate_.get();
}

auto agent::host::scratch_context() -> v8::Local<v8::Context> {
	if (scratch_context_.IsEmpty()) {
		auto context = v8::Context::New(isolate_.get());
		scratch_context_.Reset(isolate_.get(), context);
		scratch_context_.SetWeak();
		return context;
	} else {
		return scratch_context_.Get(isolate_.get());
	}
}

auto agent::agent::schedule_task(task_type task) -> void {
	// auto agent_host::schedule_tasks(const std::shared_ptr<agent_host>& host, std::ranges::range auto tasks) -> void {
	// 	auto locked = pending_tasks.write_notify();
	// 	locked->tasks.reserve(locked->tasks.size() + std::ranges::size(tasks));
	// 	std::ranges::move(tasks, std::back_inserter(locked->tasks));
	// 	locked->host = host;
	// }
	auto host = this->host_.lock();
	if (host) {
		host->pending_tasks_.write_notify()->push_back(std::move(task));
	}
}

auto isolate_destructor::operator()(v8::Isolate* isolate) -> void {
	isolate->Dispose();
}

} // namespace ivm
