#pragma once
#include <v8.h>
#include "environment.h"
#include "holder.h"
#include "util.h"
#include <memory>

namespace ivm {

/**
 * Most operations in this library can be decomposed into three phases.
 *
 * - Phase 1 [Isolate 1]: copy data out of current isolate
 * - Phase 2 [Isolate 2]: copy data into new isolate, run work, copy data out of isolate
 * - Phase 3 [Isolate 1]: copy results from phase 2 into the original isolate
 *
 * This class handles the locking and thread synchronization for either synchronous or
 * asynchronous functions. That way the same code can be used for both versions of each function.
 *
 * When `async=true` a promise is return which will be resolved after all the work is done
 * When `async=false` this will run in the calling thread until completion
 */
class ThreePhaseTask {
	private:
		struct Phase2Runner : public Runnable {
			std::unique_ptr<ThreePhaseTask> self;
			std::shared_ptr<IsolateHolder> first_isolate_ref;
			std::unique_ptr<v8::Persistent<v8::Promise::Resolver>> promise_persistent;
			std::unique_ptr<v8::Persistent<v8::Context>> context_persistent;
			bool did_run = false;

			Phase2Runner(
				std::unique_ptr<ThreePhaseTask> self,
				std::shared_ptr<IsolateHolder> first_isolate_ref,
				std::unique_ptr<v8::Persistent<v8::Promise::Resolver>> promise_persistent,
				std::unique_ptr<v8::Persistent<v8::Context>> context_persistent
			);
			~Phase2Runner();
			void Run() final;
		};

		v8::Local<v8::Value> RunSync(IsolateHolder& second_isolate);

	public:
		ThreePhaseTask() = default;
		ThreePhaseTask(const ThreePhaseTask&) = delete;
		ThreePhaseTask& operator= (const ThreePhaseTask&) = delete;
		virtual ~ThreePhaseTask() = default;

		virtual void Phase2() = 0;
		virtual v8::Local<v8::Value> Phase3() = 0;

		template <bool async, typename T, typename ...Args>
		static v8::Local<v8::Value> Run(IsolateHolder& second_isolate, Args&&... args) {

			if (async) {
				// Build a promise for outer isolate
				v8::Isolate* isolate = v8::Isolate::GetCurrent();
				auto context_local = isolate->GetCurrentContext();
				auto promise_local = v8::Promise::Resolver::New(isolate);
				v8::TryCatch try_catch(isolate);
				try {
					// Schedule Phase2 async
					second_isolate.ScheduleTask(
						std::make_unique<Phase2Runner>(
							std::make_unique<T>(std::forward<Args>(args)...), // <-- Phase1 / ctor called here
							IsolateEnvironment::GetCurrentHolder(),
							std::make_unique<v8::Persistent<v8::Promise::Resolver>>(isolate, promise_local),
							std::make_unique<v8::Persistent<v8::Context>>(isolate, context_local)
						), false, true
					);
				} catch (const js_runtime_error& cc_error) {
					// An error was caught while running ctor (phase 1)
					assert(try_catch.HasCaught());
					v8::Maybe<bool> ret = promise_local->Reject(context_local, try_catch.Exception());
					try_catch.Reset();
					Unmaybe(ret);
				}
				return promise_local->GetPromise();
			} else {
				// Execute syncronously
				T self(std::forward<Args>(args)...);
				return self.RunSync(second_isolate);
			}
		}
};

} // namespace ivm
