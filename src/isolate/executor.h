#pragma once
#include <v8.h>
#include <chrono>
#include <mutex>
#include <thread>

namespace ivm {
class IsolateEnvironment;

/**
 * Executor class handles v8 locking while C++ code is running. Thread syncronization is handled
 * by v8::Locker. This also enters the isolate and sets up a handle scope.
 */
class Executor { // "En taro adun"
	friend class InspectorAgent;
	private:
		struct CpuTimer {
			using TimePoint = std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>;
			struct PauseScope {
				CpuTimer* timer;
				explicit PauseScope(CpuTimer* timer);
				PauseScope(const PauseScope&) = delete;
				PauseScope& operator=(const PauseScope&) = delete;
				~PauseScope();
			};
			struct UnpauseScope {
				CpuTimer* timer;
				explicit UnpauseScope(PauseScope& pause);
				UnpauseScope(const UnpauseScope&) = delete;
				UnpauseScope& operator=(const UnpauseScope&) = delete;
				~UnpauseScope();
			};
			Executor& executor;
			CpuTimer* last;
			TimePoint time;
			explicit CpuTimer(Executor& executor);
			CpuTimer(const CpuTimer&) = delete;
			CpuTimer operator= (const CpuTimer&) = delete;
			~CpuTimer();
			std::chrono::nanoseconds Delta(const std::lock_guard<std::mutex>& /* lock */) const;
			void Pause();
			void Resume();
			static TimePoint Now();
		};

		// WallTimer is also responsible for pausing the current CpuTimer before we attempt to
		// acquire the v8::Locker, because that could block in which case CPU shouldn't be counted.
		struct WallTimer {
			Executor& executor;
			CpuTimer* cpu_timer;
			std::chrono::time_point<std::chrono::steady_clock> time;
			explicit WallTimer(Executor& executor);
			WallTimer(const WallTimer&) = delete;
			WallTimer operator= (const WallTimer&) = delete;
			~WallTimer();
			std::chrono::nanoseconds Delta(const std::lock_guard<std::mutex>& /* lock */) const;
		};

	public:
		class Scope {
			private:
				IsolateEnvironment* last;

			public:
				explicit Scope(IsolateEnvironment& env);
				Scope(const Scope&) = delete;
				Scope operator= (const Scope&) = delete;
				~Scope();
		};

		class Lock {
			private:
				// These need to be separate from `Executor::current` because the default isolate
				// doesn't actually get a lock.
				static thread_local Lock* current;
				Lock* last;
				Scope scope;
				WallTimer wall_timer;
				v8::Locker locker;
				CpuTimer cpu_timer;
				v8::Isolate::Scope isolate_scope;
				v8::HandleScope handle_scope;

			public:
				explicit Lock(IsolateEnvironment& env);
				Lock(const Lock&) = delete;
				Lock operator= (const Lock&) = delete;
				~Lock();
		};

		class Unlock {
			private:
				CpuTimer::PauseScope pause_scope;
				v8::Unlocker unlocker;

			public:
				explicit Unlock(IsolateEnvironment& env);
				Unlock(const Unlock&) = delete;
				Unlock operator= (const Unlock&) = delete;
				~Unlock();
		};

		static thread_local IsolateEnvironment* current_env;
		static std::thread::id default_thread;
		IsolateEnvironment& env;
		Lock* current_lock = nullptr;
		static thread_local CpuTimer* cpu_timer_thread;
		CpuTimer* cpu_timer = nullptr;
		WallTimer* wall_timer = nullptr;
		std::mutex timer_mutex;
		std::chrono::nanoseconds cpu_time{};
		std::chrono::nanoseconds wall_time{};

	public:
		explicit Executor(IsolateEnvironment& env);
		Executor(const Executor&) = delete;
		Executor operator= (const Executor&) = delete;
		~Executor() = default;
		static void Init(IsolateEnvironment& default_isolate);
		static IsolateEnvironment* GetCurrent() { return current_env; }
		static bool IsDefaultThread();
};

} // namespace ivm
