export module v8_js:isolated.platform;
import :platform.delegate;
import :platform.thread_cache;
import std;
import v8;

namespace js::iv8::isolated {

class isolated_agent_delegate_storage {
	public:
		explicit isolated_agent_delegate_storage(unsigned concurrency) :
				thread_cache_{concurrency} {}
		auto foreground_thread_cache() -> platform::thread_cache& { return thread_cache_; }

	protected:
		auto worker_thread_cache() -> platform::thread_cache& { return thread_cache_; }

	private:
		platform::thread_cache thread_cache_;
};

class isolated_agent_delegate
		: virtual public v8::Platform,
			public isolated_agent_delegate_storage,
			public platform::background_worker_delegate,
			public platform::instrumentation_delegate,
			public platform::platform_entropy_delegate<isolated_agent_delegate> {
	public:
		explicit isolated_agent_delegate(unsigned concurrency) :
				isolated_agent_delegate_storage{concurrency},
				platform::background_worker_delegate{worker_thread_cache()} {}

		auto CurrentClockTimeMilliseconds() -> std::int64_t override;
		auto CurrentClockTimeMillis() -> double override;
		auto GetForegroundTaskRunner(v8::Isolate* isolate, v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner> override;

		static auto fill_random_bytes_for_isolate(v8::Isolate* isolate, unsigned char* buffer, std::size_t length) -> bool;
};

class isolated_platform final
		: public isolated_agent_delegate,
			public platform::initialization_delegate {
	public:
		explicit isolated_platform(unsigned concurrency) :
				isolated_agent_delegate{concurrency},
				initialization_delegate{this} {}
		static auto acquire(unsigned concurrency) -> platform::platform_handle;
};

} // namespace js::iv8::isolated
