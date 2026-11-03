export module v8_js:platform.thread_cache;
import boost;
import std;
import util;

namespace js::iv8::platform {

export class runner_thread;
export class thread_cache;

// RAII registration of a thread which is about to suspend.
export class thread_suspension : util::non_moveable {
	public:
		explicit thread_suspension(runner_thread& thread);
		~thread_suspension();

		[[nodiscard]] auto get_token() const noexcept -> util::inplace_stop_token { return stop_source_.get_token(); }
		[[nodiscard]] auto reclaimed() const noexcept -> bool { return stop_source_.stop_requested(); }

	private:
		friend thread_cache;
		using intrusive_safe_mode = boost::intrusive::link_mode<boost::intrusive::safe_link>;
		using intrusive_list_hook = boost::intrusive::list_member_hook<intrusive_safe_mode>;

		intrusive_list_hook list_hook_;
		std::reference_wrapper<runner_thread> thread_;
		util::inplace_stop_source stop_source_;
		std::atomic<bool> stopped_;

	public:
		using intrusive_hook = boost::intrusive::member_hook<thread_suspension, intrusive_list_hook, &thread_suspension::list_hook_>;
};

// A thread owned by `thread_cache`. Runs one job at a time and returns itself to the cache after.
export class runner_thread : util::non_moveable {
	public:
		using job_type = util::move_only_function<auto(runner_thread&)->void>;

		explicit runner_thread(thread_cache& cache);
		~runner_thread();

		[[nodiscard]] auto id() const -> std::thread::id { return thread_.get_id(); }
		[[nodiscard]] auto reassigned() const -> bool { return pending_.load(std::memory_order_acquire); }
		auto will_suspend() -> thread_suspension { return thread_suspension{*this}; }

	private:
		friend thread_cache;
		friend thread_suspension;
		auto assign(job_type work) -> void;
		auto run() -> void;

		using intrusive_safe_mode = boost::intrusive::link_mode<boost::intrusive::safe_link>;
		using intrusive_list_hook = boost::intrusive::slist_member_hook<intrusive_safe_mode>;

		intrusive_list_hook list_hook_;
		std::reference_wrapper<thread_cache> cache_;
		std::binary_semaphore semaphore_{0};
		std::atomic<bool> pending_;
		job_type work_;
		std::thread thread_;

	public:
		using intrusive_hook = boost::intrusive::member_hook<runner_thread, intrusive_list_hook, &runner_thread::list_hook_>;
};

// Pool of reusable threads shared by the schedulers of a platform.
export class thread_cache : util::non_moveable {
	public:
		enum class request_kind : bool {
			soft,
			hard,
		};

		thread_cache();
		~thread_cache();

		[[nodiscard]] auto concurrency() const -> unsigned { return concurrency_; }
		auto request(request_kind kind, runner_thread::job_type work) -> std::thread::id;

	private:
		friend runner_thread;
		friend thread_suspension;
		using intrusive_no_size = boost::intrusive::constant_time_size<false>;
		using cached_list_type = boost::intrusive::slist<runner_thread, intrusive_no_size, runner_thread::intrusive_hook>;
		using suspended_list_type = boost::intrusive::list<thread_suspension, intrusive_no_size, thread_suspension::intrusive_hook>;

		struct storage {
				std::list<runner_thread> threads;
				cached_list_type cached;
				suspended_list_type suspended;
				unsigned active = 0;
		};

		auto release(runner_thread& thread) -> void;
		auto resume(thread_suspension& suspension) -> void;
		auto suspend(thread_suspension& suspension) -> void;

		util::lockable<storage> storage_;
		unsigned concurrency_;
};

} // namespace js::iv8::platform
