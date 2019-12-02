#pragma once
#include <v8.h>
#include <chrono>
#include <mutex>
#include <thread>

namespace ivm {
class InspectorAgent;
class IsolateEnvironment;

/**
 * Executor class handles v8 locking while C++ code is running. Thread syncronization is handled
 * by v8::Locker. This also enters the isolate and sets up a handle scope.
 */
class Executor { // "En taro adun"
	friend InspectorAgent;
	friend IsolateEnvironment;
	private:
		class CpuTimer {
			public:
				explicit CpuTimer(Executor& executor);
				CpuTimer(const CpuTimer&) = delete;
				~CpuTimer();
				auto operator= (const CpuTimer&) = delete;

				using TimePoint = std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>;
				auto Delta(const std::lock_guard<std::mutex>& /* lock */) const -> std::chrono::nanoseconds;
				void Pause();
				void Resume();
				static auto Now() -> TimePoint;

			private:
				Executor& executor;
				CpuTimer* last;
				TimePoint time;
		};

		// WallTimer is also responsible for pausing the current CpuTimer before we attempt to
		// acquire the v8::Locker, because that could block in which case CPU shouldn't be counted.
		class WallTimer {
			public:
				explicit WallTimer(Executor& executor);
				WallTimer(const WallTimer&) = delete;
				~WallTimer();
				auto operator= (const WallTimer&) = delete;

				auto Delta(const std::lock_guard<std::mutex>& /* lock */) const -> std::chrono::nanoseconds;

			private:
				Executor& executor;
				CpuTimer* cpu_timer;
				std::chrono::time_point<std::chrono::steady_clock> time;
		};

	public:
		explicit Executor(IsolateEnvironment& env) : env{env} {};
		Executor(const Executor&) = delete;
		~Executor() = default;
		auto operator= (const Executor&) = delete;

		static void Init(IsolateEnvironment& default_isolate);
		static auto GetCurrent() { return current_env; }
		static auto IsDefaultThread() { return std::this_thread::get_id() == default_thread; };

		// Pauses CpuTimer
		class UnpauseScope;
		class PauseScope {
			friend UnpauseScope;
			public:
				explicit PauseScope(CpuTimer* timer) : timer{timer} { timer->Pause(); }
				PauseScope(const PauseScope&) = delete;
				~PauseScope() { timer->Resume(); }
				auto operator=(const PauseScope&) = delete;

			private:
				CpuTimer* timer;
		};

		// Unpauses CpuTimer
		class UnpauseScope {
			public:
				explicit UnpauseScope(PauseScope& pause) : timer{pause.timer} { timer->Resume(); }
				UnpauseScope(const UnpauseScope&) = delete;
				~UnpauseScope() { timer->Pause(); }
				auto operator=(const UnpauseScope&) = delete;

			public:
				CpuTimer* timer;
		};

		// A scope sets the current environment without locking v8
		class Scope {
			public:
				explicit Scope(IsolateEnvironment& env) : last{current_env} { current_env = &env; }
				Scope(const Scope&) = delete;
				~Scope() { current_env = last; }
				auto operator= (const Scope&) = delete;

			private:
				IsolateEnvironment* last;
		};

		// Locks this environment for execution. Implies `Scope` as well.
		class Lock {
			public:
				explicit Lock(IsolateEnvironment& env);
				Lock(const Lock&) = delete;
				~Lock();
				auto operator= (const Lock&) = delete;

			private:
				// These need to be separate from `Executor::current` because the default isolate
				// doesn't actually get a lock.
				Lock* last;
				Scope scope;
				WallTimer wall_timer;
				v8::Locker locker;
				CpuTimer cpu_timer;
				v8::Isolate::Scope isolate_scope;
				v8::HandleScope handle_scope;

				static thread_local Lock* current;
		};

		class Unlock {
			public:
				explicit Unlock(IsolateEnvironment& env);
				Unlock(const Unlock&) = delete;
				~Unlock();
				auto operator= (const Unlock&) = delete;

			private:
				PauseScope pause_scope;
				v8::Unlocker unlocker;
		};

	private:
		IsolateEnvironment& env;
		Lock* current_lock = nullptr;
		CpuTimer* cpu_timer = nullptr;
		WallTimer* wall_timer = nullptr;
		std::mutex timer_mutex;
		std::chrono::nanoseconds cpu_time{};
		std::chrono::nanoseconds wall_time{};

		static std::thread::id default_thread;
		static thread_local IsolateEnvironment* current_env;
		static thread_local CpuTimer* cpu_timer_thread;
};

} // namespace ivm
