#pragma once
#include "runnable.h"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <v8.h>

namespace ivm {

class IsolateEnvironment;
class timer_t;

class PromiseTimeout : public std::enable_shared_from_this<PromiseTimeout> {
	public:
		PromiseTimeout(IsolateEnvironment& env, uint32_t timeout_ms);
		~PromiseTimeout();
		PromiseTimeout(const PromiseTimeout&) = delete;
		auto operator=(const PromiseTimeout&) = delete;

		void PromiseCreated();
		void PromiseResolved();
		void Watch(const void* owner, std::function<void()> callback);
		void Unwatch(const void* owner);
		void BeforeReaction();
		void AfterReaction();
		auto IsExpired() -> bool;

	private:
		static void UpdateUvRef(const std::weak_ptr<IsolateEnvironment>& default_env, bool acquire);
		void StartTimer();
		void StopIfUnused();
		void Expire();
		void Notify();
		void RunNotification(bool interrupt);
		void PostNotification(bool interrupt);
		void PostUvRef(bool acquire);
		void ReleaseUvRef();

		IsolateEnvironment& env;
		std::weak_ptr<IsolateEnvironment> weak_env;
		std::weak_ptr<IsolateEnvironment> default_env;
		std::weak_ptr<v8::TaskRunner> default_task_runner;
		std::chrono::steady_clock::time_point deadline;
		std::unique_ptr<timer_t> timer;
		mutable std::mutex mutex;
		std::unordered_map<const void*, std::function<void()>> watchers;
		size_t pending_promises = 0;
		unsigned int reaction_depth = 0;
		bool expired = false;
		bool did_terminate = false;
		bool on_default_thread = false;
		bool has_uv_ref = false;
};

class PromiseTimeoutScope {
	public:
		PromiseTimeoutScope(IsolateEnvironment& env, uint32_t timeout_ms);
		~PromiseTimeoutScope();
		PromiseTimeoutScope(const PromiseTimeoutScope&) = delete;
		auto operator=(const PromiseTimeoutScope&) = delete;

	private:
		IsolateEnvironment& env;
		std::shared_ptr<PromiseTimeout> previous;
		std::shared_ptr<PromiseTimeout> current;
		bool engaged = false;
};

struct PromiseTimeoutReference {
	explicit PromiseTimeoutReference(std::shared_ptr<PromiseTimeout> timeout);
	~PromiseTimeoutReference();
	void Resolve();

	std::shared_ptr<PromiseTimeout> timeout;
	v8::Persistent<v8::Value> external;
	bool resolved = false;
};

} // namespace ivm
